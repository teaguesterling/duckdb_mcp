#pragma once

#include "duckdb.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/file_buffer.hpp"
#include "mcpfs/mcp_path.hpp"

namespace duckdb {

// Forward declarations
class MCPConnection;
class MCPTransport;

// MCP File Handle for streaming reads
class MCPFileHandle : public FileHandle {
public:
	MCPFileHandle(FileSystem &file_system, const string &path, FileOpenFlags flags,
	              shared_ptr<MCPConnection> connection, const MCPPath &mcp_path);
	~MCPFileHandle() override = default;

	// FileHandle interface
	void Close() override;

public:
	// Public for FileSystem access
	shared_ptr<MCPConnection> connection;
	MCPPath parsed_path;
	string resource_content;
	bool content_loaded;
	idx_t current_position;

	void LoadResourceContent();
};

// Main MCP File System implementation
class MCPFileSystem : public FileSystem {
public:
	MCPFileSystem();
	~MCPFileSystem() override = default;

	// Core file operations
	unique_ptr<FileHandle> OpenFile(const string &path, FileOpenFlags flags,
	                                optional_ptr<FileOpener> opener = nullptr) override;

	// File I/O operations
	int64_t Read(FileHandle &handle, void *buffer, int64_t nr_bytes) override;
	int64_t Write(FileHandle &handle, void *buffer, int64_t nr_bytes) override;
	void Read(FileHandle &handle, void *buffer, int64_t nr_bytes, idx_t location) override;
	void Write(FileHandle &handle, void *buffer, int64_t nr_bytes, idx_t location) override;

	// File metadata operations
	bool FileExists(const string &filename, optional_ptr<FileOpener> opener = nullptr) override;
	int64_t GetFileSize(FileHandle &handle) override;
	timestamp_t GetLastModifiedTime(FileHandle &handle) override;
	FileType GetFileType(FileHandle &handle) override;

	// Seeking operations
	void Seek(FileHandle &handle, idx_t location) override;
	void Reset(FileHandle &handle) override;
	idx_t SeekPosition(FileHandle &handle) override;
	bool CanSeek() override;

	// File system characteristics
	bool OnDiskFile(FileHandle &handle) override;

	// Directory operations
	bool DirectoryExists(const string &directory, optional_ptr<FileOpener> opener = nullptr) override;
	void CreateDirectory(const string &directory, optional_ptr<FileOpener> opener = nullptr) override;
	void RemoveDirectory(const string &directory, optional_ptr<FileOpener> opener = nullptr) override;
	bool ListFiles(const string &directory, const std::function<void(const string &, bool)> &callback,
	               FileOpener *opener = nullptr) override;

	// Glob pattern support
	vector<OpenFileInfo> Glob(const string &path, FileOpener *opener = nullptr) override;

	// Move/rename operations (not supported for MCP resources)
	void MoveFile(const string &source, const string &target, optional_ptr<FileOpener> opener = nullptr) override;
	void RemoveFile(const string &filename, optional_ptr<FileOpener> opener = nullptr) override;

	// File system identification
	string GetName() const override {
		return "MCPFileSystem";
	}
	bool CanHandleFile(const string &fpath) override;

	// MCP-specific operations
	shared_ptr<MCPConnection> GetConnection(const string &server_name);

private:
	// Helper methods
	MCPPath ValidateAndParsePath(const string &path);
	void EnsureConnectionExists(const string &server_name);
};

} // namespace duckdb
