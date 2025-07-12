#include "mcpfs/mcp_file_system.hpp"
#include "protocol/mcp_connection.hpp"
#include "client/mcp_storage_extension.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"

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
        auto resource = connection->ReadResource(parsed_path.full_mcp_uri);
        resource_content = resource.content;
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
    if (!connection->ResourceExists(parsed_path.full_mcp_uri)) {
        throw IOException("MCP resource not found: " + parsed_path.full_mcp_uri);
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
    
    memcpy(buffer, mcp_handle.resource_content.data() + mcp_handle.current_position, to_read);
    mcp_handle.current_position += to_read;
    
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
    
    int64_t available = static_cast<int64_t>(mcp_handle.resource_content.length()) - location;
    int64_t to_read = std::min(nr_bytes, available);
    
    if (to_read > 0) {
        memcpy(buffer, mcp_handle.resource_content.data() + location, to_read);
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
        
        return connection->ResourceExists(parsed_path.full_mcp_uri);
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

time_t MCPFileSystem::GetLastModifiedTime(FileHandle &handle) {
    // MCP resources don't have reliable modification times
    // Return current time as placeholder
    return time(nullptr);
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
    
    mcp_handle.current_position = location;
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
        
        // List all resources and filter by pattern
        auto resources = connection->ListResources();
        
        for (const auto &resource : resources) {
            string full_path = MCPPathParser::ConstructPath(parsed_path.server_name, resource.uri);
            
            // Simple pattern matching - would implement proper glob matching
            if (StringUtil::Contains(resource.uri, parsed_path.resource_path) ||
                StringUtil::Contains(full_path, path)) {
                OpenFileInfo info;
                info.path = full_path;
                // Note: OpenFileInfo doesn't have a size field
                // Size information would need to be stored in extended_info if needed
                results.push_back(info);
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
    return MCPConnectionRegistry::GetInstance().GetConnection(server_name);
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