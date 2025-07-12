#pragma once

#include "duckdb.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include "duckdb/parser/parsed_data/attach_info.hpp"
#include "duckdb/main/attached_database.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_entry/schema_catalog_entry.hpp"
#include "duckdb/common/enums/on_entry_not_found.hpp"

namespace duckdb {

// Forward declarations
class MCPConnection;

// MCP Storage Extension for handling ATTACH statements
class MCPStorageExtension {
public:
    static unique_ptr<StorageExtension> Create();
    
private:
    // Main attach function called when ATTACH ... (TYPE mcp) is executed
    static unique_ptr<Catalog> MCPStorageAttach(StorageExtensionInfo *storage_info,
                                               ClientContext &context,
                                               AttachedDatabase &db,
                                               const string &name,
                                               AttachInfo &info,
                                               AccessMode access_mode);
    
    // Transaction manager creator
    static unique_ptr<TransactionManager> MCPStorageTransactionManager(StorageExtensionInfo *storage_info,
                                                                       AttachedDatabase &db,
                                                                       Catalog &catalog);
    
    // Helper methods
    static shared_ptr<MCPConnection> CreateMCPConnection(const AttachInfo &info);
    static void RegisterMCPConnection(ClientContext &context, const string &name, 
                                     shared_ptr<MCPConnection> connection);
};

// Simple MCP connection registry for storing MCP connections
class MCPConnectionRegistry {
public:
    static MCPConnectionRegistry& GetInstance();
    
    void RegisterConnection(const string &name, shared_ptr<MCPConnection> connection);
    void UnregisterConnection(const string &name);
    shared_ptr<MCPConnection> GetConnection(const string &name);
    
private:
    mutex registry_mutex;
    unordered_map<string, shared_ptr<MCPConnection>> connections;
};

} // namespace duckdb