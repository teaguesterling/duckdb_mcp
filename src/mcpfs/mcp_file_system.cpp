#include "mcpfs/mcp_file_system.hpp"
#include "protocol/mcp_connection.hpp"
#include "client/mcp_storage_extension.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/open_file_info.hpp"
#include "duckdb/common/types/timestamp.hpp"

namespace duckdb {

// MCPFileHandle implementation

MCPFileHandle::MCPFileHandle(FileSystem &file_system, const string &path, FileOpenFlags flags,
                           shared_ptr<MCPConnection> connection, const MCPPath &mcp_path)
    : FileHandle(file_system, path, flags), connection(connection), parsed_path(mcp_path),
      content_loaded(false), current_position(0) {
}

void MCPFileHandle::Close() {
    // Nothing special needed for MCP file handles
}

void MCPFileHandle::LoadResourceContent() {
    if (content_loaded) {
        return;
    }
    
    if (!connection || !connection->IsInitialized()) {
        throw IOException("MCP connection not available for path: " + path);
    }
    
    try {
        auto resource = connection->ReadResource(parsed_path.resource_uri);
        
        // Extract the actual text content from the MCP JSON response
        string json_response = resource.content;
        string extracted_content;
        
        // Look for pattern: "text":"..."
        auto text_pos = json_response.find("\"text\":\"");
        if (text_pos != string::npos) {
            text_pos += 8; // len("\"text\":\"")
            auto text_end = text_pos;
            
            // Find the end of the text field, handling escaped quotes
            bool escaped = false;
            for (size_t i = text_pos; i < json_response.length(); i++) {
                char c = json_response[i];
                if (escaped) {
                    escaped = false;
                    continue;
                }
                if (c == '\\') {
                    escaped = true;
                    continue;
                }
                if (c == '"') {
                    text_end = i;
                    break;
                }
            }
            
            if (text_end > text_pos) {
                string escaped_text = json_response.substr(text_pos, text_end - text_pos);
                
                // Unescape the content (handle \n, \t, \\, \")
                for (size_t i = 0; i < escaped_text.length(); i++) {
                    if (escaped_text[i] == '\\' && i + 1 < escaped_text.length()) {
                        char next = escaped_text[i + 1];
                        if (next == 'n') {
                            extracted_content += '\n';
                            i++; // Skip the next character
                        } else if (next == 't') {
                            extracted_content += '\t';
                            i++; // Skip the next character
                        } else if (next == 'r') {
                            extracted_content += '\r';
                            i++; // Skip the next character
                        } else if (next == '\\') {
                            extracted_content += '\\';
                            i++; // Skip the next character
                        } else if (next == '"') {
                            extracted_content += '"';
                            i++; // Skip the next character
                        } else {
                            extracted_content += escaped_text[i];
                        }
                    } else {
                        extracted_content += escaped_text[i];
                    }
                }
                
                resource_content = extracted_content;
            } else {
                // Fallback: use the entire JSON response
                resource_content = json_response;
            }
        } else {
            // Fallback: use the entire JSON response
            resource_content = json_response;
        }
        
        content_loaded = true;
        current_position = 0;
    } catch (const std::exception &e) {
        throw IOException("Failed to load MCP resource: " + string(e.what()));
    }
}

// MCPFileSystem implementation

MCPFileSystem::MCPFileSystem() {
}

unique_ptr<FileHandle> MCPFileSystem::OpenFile(const string &path, FileOpenFlags flags, 
                                             optional_ptr<FileOpener> opener) {
    auto parsed_path = ValidateAndParsePath(path);
    auto connection = GetConnection(parsed_path.server_name);
    
    if (!connection) {
        throw IOException("No MCP server connection found for: " + parsed_path.server_name);
    }
    
    if (!connection->IsInitialized()) {
        if (!connection->Initialize()) {
            throw IOException("Failed to initialize MCP connection: " + connection->GetLastError());
        }
    }
    
    // Check if resource exists
    if (!connection->ResourceExists(parsed_path.resource_uri)) {
        throw IOException("MCP resource not found: " + parsed_path.resource_uri);
    }
    
    return make_uniq<MCPFileHandle>(*this, path, flags, connection, parsed_path);
}

int64_t MCPFileSystem::Read(FileHandle &handle, void *buffer, int64_t nr_bytes) {
    auto &mcp_handle = static_cast<MCPFileHandle&>(handle);
    
    if (!mcp_handle.content_loaded) {
        mcp_handle.LoadResourceContent();
    }
    
    if (mcp_handle.current_position >= static_cast<int64_t>(mcp_handle.resource_content.length())) {
        return 0; // EOF
    }
    
    int64_t available = static_cast<int64_t>(mcp_handle.resource_content.length()) - mcp_handle.current_position;
    int64_t to_read = std::min(nr_bytes, available);
    
    memcpy(buffer, mcp_handle.resource_content.data() + mcp_handle.current_position, static_cast<size_t>(to_read));
    mcp_handle.current_position += static_cast<idx_t>(to_read);
    
    return to_read;
}

int64_t MCPFileSystem::Write(FileHandle &handle, void *buffer, int64_t nr_bytes) {
    throw NotImplementedException("Writing to MCP resources is not supported");
}

void MCPFileSystem::Read(FileHandle &handle, void *buffer, int64_t nr_bytes, idx_t location) {
    auto &mcp_handle = static_cast<MCPFileHandle&>(handle);
    
    if (!mcp_handle.content_loaded) {
        mcp_handle.LoadResourceContent();
    }
    
    if (location >= mcp_handle.resource_content.length()) {
        throw IOException("Read location beyond file size");
    }
    
    int64_t available = static_cast<int64_t>(mcp_handle.resource_content.length()) - static_cast<int64_t>(location);
    int64_t to_read = std::min(nr_bytes, available);
    
    if (to_read > 0) {
        memcpy(buffer, mcp_handle.resource_content.data() + location, static_cast<size_t>(to_read));
    }
    
    if (to_read < nr_bytes) {
        throw IOException("Could not read all requested bytes from MCP resource");
    }
}

void MCPFileSystem::Write(FileHandle &handle, void *buffer, int64_t nr_bytes, idx_t location) {
    throw NotImplementedException("Writing to MCP resources is not supported");
}

bool MCPFileSystem::FileExists(const string &filename, optional_ptr<FileOpener> opener) {
    try {
        auto parsed_path = ValidateAndParsePath(filename);
        auto connection = GetConnection(parsed_path.server_name);
        
        if (!connection || !connection->IsInitialized()) {
            return false;
        }
        
        return connection->ResourceExists(parsed_path.resource_uri);
    } catch (...) {
        return false;
    }
}

int64_t MCPFileSystem::GetFileSize(FileHandle &handle) {
    auto &mcp_handle = static_cast<MCPFileHandle&>(handle);
    
    if (!mcp_handle.content_loaded) {
        mcp_handle.LoadResourceContent();
    }
    
    return static_cast<int64_t>(mcp_handle.resource_content.length());
}

timestamp_t MCPFileSystem::GetLastModifiedTime(FileHandle &handle) {
    // MCP resources don't have reliable modification times
    // Return current time as placeholder
    return Timestamp::GetCurrentTimestamp();
}

FileType MCPFileSystem::GetFileType(FileHandle &handle) {
    // All MCP resources are treated as regular files
    return FileType::FILE_TYPE_REGULAR;
}

void MCPFileSystem::Seek(FileHandle &handle, idx_t location) {
    auto &mcp_handle = static_cast<MCPFileHandle&>(handle);
    
    if (!mcp_handle.content_loaded) {
        mcp_handle.LoadResourceContent();
    }
    
    if (location > mcp_handle.resource_content.length()) {
        throw IOException("Seek location beyond file size");
    }
    
    mcp_handle.current_position = static_cast<idx_t>(location);
}

void MCPFileSystem::Reset(FileHandle &handle) {
    auto &mcp_handle = static_cast<MCPFileHandle&>(handle);
    mcp_handle.current_position = 0;
}

idx_t MCPFileSystem::SeekPosition(FileHandle &handle) {
    auto &mcp_handle = static_cast<MCPFileHandle&>(handle);
    return mcp_handle.current_position;
}

bool MCPFileSystem::CanSeek() {
    return true; // MCP resources are loaded into memory, so seeking is supported
}

bool MCPFileSystem::OnDiskFile(FileHandle &handle) {
    return false; // MCP files are virtual, not stored on disk
}

bool MCPFileSystem::DirectoryExists(const string &directory, optional_ptr<FileOpener> opener) {
    // MCP doesn't have traditional directories
    return false;
}

void MCPFileSystem::CreateDirectory(const string &directory, optional_ptr<FileOpener> opener) {
    throw NotImplementedException("Creating directories in MCP is not supported");
}

void MCPFileSystem::RemoveDirectory(const string &directory, optional_ptr<FileOpener> opener) {
    throw NotImplementedException("Removing directories in MCP is not supported");
}

bool MCPFileSystem::ListFiles(const string &directory, const std::function<void(const string &, bool)> &callback,
                            FileOpener *opener) {
    // Would implement directory listing via MCP resource enumeration
    return false;
}

vector<OpenFileInfo> MCPFileSystem::Glob(const string &path, FileOpener *opener) {
    vector<OpenFileInfo> results;
    
    try {
        auto parsed_path = ValidateAndParsePath(path);
        auto connection = GetConnection(parsed_path.server_name);
        
        if (!connection || !connection->IsInitialized()) {
            return results;
        }
        
        // For exact path matching, first check if the specific resource exists
        if (connection->ResourceExists(parsed_path.resource_uri)) {
            OpenFileInfo info(path);  // Use the original path as requested
            results.push_back(info);
        } else {
            // List all resources and filter by pattern
            auto resources = connection->ListResources();
            
            for (const auto &resource : resources) {
                string full_path = MCPPathParser::ConstructPath(parsed_path.server_name, resource.uri);
                
                // Simple pattern matching - would implement proper glob matching
                if (StringUtil::Contains(resource.uri, parsed_path.resource_uri) ||
                    StringUtil::Contains(full_path, path)) {
                    OpenFileInfo info(full_path);
                    // Note: OpenFileInfo doesn't have a size field
                    // Size information would need to be stored in extended_info if needed
                    results.push_back(info);
                }
            }
        }
    } catch (...) {
        // Return empty results on error
    }
    
    return results;
}

void MCPFileSystem::MoveFile(const string &source, const string &target, optional_ptr<FileOpener> opener) {
    throw NotImplementedException("Moving MCP files is not supported");
}

void MCPFileSystem::RemoveFile(const string &filename, optional_ptr<FileOpener> opener) {
    throw NotImplementedException("Removing MCP files is not supported");
}

bool MCPFileSystem::CanHandleFile(const string &fpath) {
    return MCPPathParser::IsValidMCPPath(fpath);
}

shared_ptr<MCPConnection> MCPFileSystem::GetConnection(const string &server_name) {
    // Get connection from the global registry
    auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
    if (!connection) {
        throw IOException("No MCP connection found in registry for server: '" + server_name + "'");
    }
    return connection;
}

MCPPath MCPFileSystem::ValidateAndParsePath(const string &path) {
    if (!MCPPathParser::IsValidMCPPath(path)) {
        throw InvalidInputException("Invalid MCP path format: " + path);
    }
    
    return MCPPathParser::ParsePath(path);
}

void MCPFileSystem::EnsureConnectionExists(const string &server_name) {
    auto connection = GetConnection(server_name);
    if (!connection) {
        throw InvalidInputException("MCP server not attached: " + server_name + 
                                  ". Use ATTACH to connect to MCP server first.");
    }
}

} // namespace duckdb