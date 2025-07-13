#include "client/mcp_storage_extension.hpp"
#include "protocol/mcp_connection.hpp"
#include "protocol/mcp_transport.hpp"
#include "mcpfs/mcp_file_system.hpp"
#include "catalog/mcp_catalog.hpp"
#include "client/mcp_transaction_manager.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/parser/parsed_data/create_schema_info.hpp"
#include "duckdb/catalog/catalog_entry/schema_catalog_entry.hpp"
#include "duckdb/storage/standard_buffer_manager.hpp"
#include "duckdb/storage/object_cache.hpp"
#include "duckdb/transaction/duck_transaction_manager.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/constants.hpp"

namespace duckdb {

unique_ptr<StorageExtension> MCPStorageExtension::Create() {
    auto result = make_uniq<StorageExtension>();
    result->attach = MCPStorageAttach;
    result->create_transaction_manager = MCPStorageTransactionManager;
    return result;
}

unique_ptr<Catalog> MCPStorageExtension::MCPStorageAttach(StorageExtensionInfo *storage_info,
                                                         ClientContext &context,
                                                         AttachedDatabase &db,
                                                         const string &name,
                                                         AttachInfo &info,
                                                         AccessMode access_mode) {
    
    // Create MCP connection from attach info
    auto mcp_connection = CreateMCPConnection(info);
    
    // Attempt to connect and initialize
    if (!mcp_connection->Connect()) {
        throw IOException("Failed to connect to MCP server: " + mcp_connection->GetLastError());
    }
    
    if (!mcp_connection->Initialize()) {
        throw IOException("Failed to initialize MCP connection: " + mcp_connection->GetLastError());
    }
    
    // Register the connection with our registry and MCPFS
    RegisterMCPConnection(context, name, mcp_connection);
    
    // Create and return MCP catalog
    auto catalog = make_uniq<MCPCatalog>(db, mcp_connection);
    catalog->Initialize(false); // Don't load builtin functions
    
    return std::move(catalog);
}

unique_ptr<TransactionManager> MCPStorageExtension::MCPStorageTransactionManager(StorageExtensionInfo *storage_info,
                                                                                 AttachedDatabase &db,
                                                                                 Catalog &catalog) {
    // MCP is read-only, use minimal transaction manager
    return make_uniq<MCPTransactionManager>(db);
}

shared_ptr<MCPConnection> MCPStorageExtension::CreateMCPConnection(const AttachInfo &info) {
    string connection_string = info.path;
    
    // Parse connection string - expect format like "stdio:///path/to/server"
    if (!StringUtil::StartsWith(StringUtil::Lower(connection_string), "stdio://")) {
        throw InvalidInputException("Currently only stdio:// transport is supported. Got: " + connection_string);
    }
    
    // Extract command line from stdio:///path/to/server
    string command_line = connection_string.substr(8); // Remove "stdio://"
    if (command_line.empty()) {
        throw InvalidInputException("Empty command path in stdio connection string");
    }
    
    // Parse command line into executable and arguments
    // Simple parsing: first token is command, rest are arguments
    stringstream ss(command_line);
    string token;
    vector<string> tokens;
    while (ss >> token) {
        tokens.push_back(token);
    }
    
    if (tokens.empty()) {
        throw InvalidInputException("Invalid command in stdio connection string");
    }
    
    // Create transport configuration
    StdioTransport::StdioConfig transport_config;
    transport_config.command_path = tokens[0];
    for (size_t i = 1; i < tokens.size(); i++) {
        transport_config.arguments.push_back(tokens[i]);
    }
    
    // Parse additional options from ATTACH parameters
    if (info.options.find("args") != info.options.end()) {
        auto args_value = info.options.at("args");
        // Would parse args array from the Value - simplified for now
        if (!args_value.IsNull()) {
            // For now, assume it's a simple string that we split on spaces
            string args_str = args_value.ToString();
            // Simple space-based splitting - would need proper JSON array parsing
            stringstream ss(args_str);
            string arg;
            while (ss >> arg) {
                transport_config.arguments.push_back(arg);
            }
        }
    }
    
    if (info.options.find("cwd") != info.options.end()) {
        auto cwd_value = info.options.at("cwd");
        if (!cwd_value.IsNull()) {
            transport_config.working_directory = cwd_value.ToString();
        }
    }
    
    if (info.options.find("timeout") != info.options.end()) {
        auto timeout_value = info.options.at("timeout");
        if (!timeout_value.IsNull()) {
            // Parse timeout - assume it's in seconds for now
            transport_config.timeout_seconds = std::stoi(timeout_value.ToString());
        }
    }
    
    // Create transport
    auto transport = make_uniq<StdioTransport>(transport_config);
    
    // Create and return MCP connection
    return make_shared_ptr<MCPConnection>(info.name, std::move(transport));
}

void MCPStorageExtension::RegisterMCPConnection(ClientContext &context, const string &name, 
                                               shared_ptr<MCPConnection> connection) {
    // Register with our connection registry
    MCPConnectionRegistry::GetInstance().RegisterConnection(name, connection);
    
    // Also register with MCPFS for file access
    auto &db = DatabaseInstance::GetDatabase(context);
    auto &fs = FileSystem::GetFileSystem(db);
    
    // Try to get MCPFS and register connection
    // This is simplified - would need proper subsystem access in real implementation
}

// MCPConnectionRegistry implementation

MCPConnectionRegistry& MCPConnectionRegistry::GetInstance() {
    static MCPConnectionRegistry instance;
    return instance;
}

void MCPConnectionRegistry::RegisterConnection(const string &name, shared_ptr<MCPConnection> connection) {
    lock_guard<mutex> lock(registry_mutex);
    connections[name] = connection;
}

void MCPConnectionRegistry::UnregisterConnection(const string &name) {
    lock_guard<mutex> lock(registry_mutex);
    connections.erase(name);
}

shared_ptr<MCPConnection> MCPConnectionRegistry::GetConnection(const string &name) {
    lock_guard<mutex> lock(registry_mutex);
    auto it = connections.find(name);
    if (it != connections.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace duckdb