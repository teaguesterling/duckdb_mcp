#include "mcpfs/mcp_file_system.hpp"
#include "mcp_instance_state.hpp"
#include "protocol/mcp_connection.hpp"
#include "client/mcp_storage_extension.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/open_file_info.hpp"
#include "duckdb/common/types/timestamp.hpp"
#include "duckdb/common/types/blob.hpp"
#include "duckdb/function/scalar/string_common.hpp"
#include "json_utils.hpp"

namespace duckdb {

namespace {

//! Extract the file body from the JSON result of an MCP `resources/read` call.
//!
//! Per the MCP spec the result is `{"contents": [ ... ]}` where each entry is
//! either a text content part (`"text"`) or a binary one (`"blob"`, base64).
//! Every part is concatenated in order, newline-separated when a part does not
//! already end in one, so line-oriented payloads (csv/jsonl) survive chunking.
//!
//! Returns false if the payload is not a recognisable `resources/read` result;
//! the caller then falls back to exposing the raw JSON, as before.
bool ExtractResourceContents(const string &json_result, string &out) {
	yyjson_doc *doc = yyjson_read(json_result.c_str(), json_result.length(), 0);
	if (!doc) {
		return false;
	}
	DocGuard guard {doc};

	yyjson_val *root = yyjson_doc_get_root(doc);
	if (!root || !yyjson_is_obj(root)) {
		return false;
	}

	yyjson_val *contents = yyjson_obj_get(root, "contents");
	if (!contents || !yyjson_is_arr(contents)) {
		return false;
	}

	string result;
	bool found_any = false;

	yyjson_arr_iter iter;
	yyjson_arr_iter_init(contents, &iter);
	yyjson_val *item;
	while ((item = yyjson_arr_iter_next(&iter))) {
		if (!yyjson_is_obj(item)) {
			continue;
		}

		string part;
		yyjson_val *text_val = yyjson_obj_get(item, "text");
		if (text_val && yyjson_is_str(text_val)) {
			part = string(yyjson_get_str(text_val), yyjson_get_len(text_val));
		} else {
			yyjson_val *blob_val = yyjson_obj_get(item, "blob");
			if (!blob_val || !yyjson_is_str(blob_val)) {
				continue;
			}
			string encoded(yyjson_get_str(blob_val), yyjson_get_len(blob_val));
			try {
				part = Blob::FromBase64(string_t(encoded));
			} catch (const std::exception &) {
				// Malformed base64: not a payload we can meaningfully decode.
				return false;
			}
		}

		if (found_any && !result.empty() && result.back() != '\n') {
			result += '\n';
		}
		result += part;
		found_any = true;
	}

	if (!found_any) {
		return false;
	}

	out = std::move(result);
	return true;
}

} // namespace

// MCPFileHandle implementation

MCPFileHandle::MCPFileHandle(FileSystem &file_system, const string &path, FileOpenFlags flags,
                             shared_ptr<MCPConnection> connection, const MCPPath &mcp_path)
    : FileHandle(file_system, path, flags), connection(connection), parsed_path(mcp_path), content_loaded(false),
      current_position(0) {
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

		// Extract the file body from the MCP `resources/read` result. If the
		// payload is not a recognisable result, expose the raw JSON unchanged
		// so callers still see *something* rather than an empty file.
		string extracted_content;
		if (ExtractResourceContents(resource.content, extracted_content)) {
			resource_content = std::move(extracted_content);
		} else {
			resource_content = resource.content;
		}

		content_loaded = true;
		current_position = 0;
	} catch (const std::exception &e) {
		throw IOException("Failed to load MCP resource: " + string(e.what()));
	}
}

// MCPFileSystem implementation

MCPFileSystem::MCPFileSystem(DatabaseInstance &db) : db_instance(db) {
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
	auto &mcp_handle = static_cast<MCPFileHandle &>(handle);

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
	auto &mcp_handle = static_cast<MCPFileHandle &>(handle);

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
	auto &mcp_handle = static_cast<MCPFileHandle &>(handle);

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
	auto &mcp_handle = static_cast<MCPFileHandle &>(handle);

	if (!mcp_handle.content_loaded) {
		mcp_handle.LoadResourceContent();
	}

	if (location > mcp_handle.resource_content.length()) {
		throw IOException("Seek location beyond file size");
	}

	mcp_handle.current_position = static_cast<idx_t>(location);
}

void MCPFileSystem::Reset(FileHandle &handle) {
	auto &mcp_handle = static_cast<MCPFileHandle &>(handle);
	mcp_handle.current_position = 0;
}

idx_t MCPFileSystem::SeekPosition(FileHandle &handle) {
	auto &mcp_handle = static_cast<MCPFileHandle &>(handle);
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
	// Errors are deliberately NOT swallowed here. This used to be wrapped in
	// `catch (...) {}`, which turned a missing server, a dead transport or a
	// JSON-RPC error into an empty match list -- indistinguishable from "the
	// server holds no matching resources".
	vector<OpenFileInfo> results;

	auto parsed_path = ValidateAndParsePath(path);
	auto connection = GetConnection(parsed_path.server_name);

	if (!connection->IsInitialized()) {
		throw IOException("MCP connection for server '" + parsed_path.server_name + "' is not initialized");
	}

	// A pattern with no wildcard names one resource. Ask for it directly rather
	// than listing every resource on the server.
	if (!FileSystem::HasGlob(parsed_path.resource_uri)) {
		if (connection->ResourceExists(parsed_path.resource_uri)) {
			results.emplace_back(path); // Use the original path as requested
		}
		return results;
	}

	// Otherwise walk EVERY page of resources/list -- a server that paginates
	// would otherwise only ever be matched against its first page -- and match
	// with real glob semantics. Substring matching was both too loose (`*.csv`
	// matching `a.csv.bak`) and too tight (`*.csv` matching nothing at all,
	// since no URI contains the literal asterisk).
	auto resources = connection->ListAllResources();

	for (const auto &resource : resources) {
		if (!duckdb::Glob(resource.uri.c_str(), resource.uri.size(), parsed_path.resource_uri.c_str(),
		                  parsed_path.resource_uri.size())) {
			continue;
		}
		results.emplace_back(MCPPathParser::ConstructPath(parsed_path.server_name, resource.uri));
		// Note: OpenFileInfo doesn't have a size field
		// Size information would need to be stored in extended_info if needed
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
	// Get connection from the per-instance registry
	auto connection = MCPInstanceState::Get(db_instance).connection_registry.GetConnection(server_name);
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
