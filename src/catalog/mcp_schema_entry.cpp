#include "catalog/mcp_schema_entry.hpp"
#include "protocol/mcp_connection.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/parser/parsed_data/create_schema_info.hpp"
#include "duckdb/parser/parsed_data/drop_info.hpp"

namespace duckdb {

MCPSchemaEntry::MCPSchemaEntry(Catalog &catalog, CreateSchemaInfo &info, shared_ptr<MCPConnection> connection)
    : SchemaCatalogEntry(catalog, info), mcp_connection(connection) {
}

MCPSchemaEntry::~MCPSchemaEntry() = default;

void MCPSchemaEntry::Scan(ClientContext &context, CatalogType type, const std::function<void(CatalogEntry &)> &callback) {
    Scan(type, callback);
}

void MCPSchemaEntry::Scan(CatalogType type, const std::function<void(CatalogEntry &)> &callback) {
    lock_guard<mutex> lock(schema_lock);
    
    // For Phase 1, scan stored entries by type
    for (auto &entry_pair : entries) {
        if (entry_pair.second->type == type) {
            callback(*entry_pair.second);
        }
    }
}

optional_ptr<CatalogEntry> MCPSchemaEntry::CreateIndex(CatalogTransaction transaction, CreateIndexInfo &info,
                                                      TableCatalogEntry &table) {
    throw NotImplementedException("CREATE INDEX is not supported for MCP schemas");
}

optional_ptr<CatalogEntry> MCPSchemaEntry::CreateFunction(CatalogTransaction transaction, CreateFunctionInfo &info) {
    throw NotImplementedException("CREATE FUNCTION is not supported for MCP schemas");
}

optional_ptr<CatalogEntry> MCPSchemaEntry::CreateTable(CatalogTransaction transaction, BoundCreateTableInfo &info) {
    throw NotImplementedException("CREATE TABLE is not supported for MCP schemas - MCP resources are read-only");
}

optional_ptr<CatalogEntry> MCPSchemaEntry::CreateView(CatalogTransaction transaction, CreateViewInfo &info) {
    throw NotImplementedException("CREATE VIEW is not supported for MCP schemas in Phase 1");
}

optional_ptr<CatalogEntry> MCPSchemaEntry::CreateSequence(CatalogTransaction transaction, CreateSequenceInfo &info) {
    throw NotImplementedException("CREATE SEQUENCE is not supported for MCP schemas");
}

optional_ptr<CatalogEntry> MCPSchemaEntry::CreateTableFunction(CatalogTransaction transaction,
                                                              CreateTableFunctionInfo &info) {
    throw NotImplementedException("CREATE FUNCTION is not supported for MCP schemas");
}

optional_ptr<CatalogEntry> MCPSchemaEntry::CreateCopyFunction(CatalogTransaction transaction,
                                                             CreateCopyFunctionInfo &info) {
    throw NotImplementedException("CREATE FUNCTION is not supported for MCP schemas");
}

optional_ptr<CatalogEntry> MCPSchemaEntry::CreatePragmaFunction(CatalogTransaction transaction,
                                                               CreatePragmaFunctionInfo &info) {
    throw NotImplementedException("CREATE FUNCTION is not supported for MCP schemas");
}

optional_ptr<CatalogEntry> MCPSchemaEntry::CreateCollation(CatalogTransaction transaction, CreateCollationInfo &info) {
    throw NotImplementedException("CREATE COLLATION is not supported for MCP schemas");
}

optional_ptr<CatalogEntry> MCPSchemaEntry::CreateType(CatalogTransaction transaction, CreateTypeInfo &info) {
    throw NotImplementedException("CREATE TYPE is not supported for MCP schemas");
}

optional_ptr<CatalogEntry> MCPSchemaEntry::LookupEntry(CatalogTransaction transaction,
                                                      const EntryLookupInfo &lookup_info) {
    lock_guard<mutex> lock(schema_lock);
    
    auto it = entries.find(lookup_info.GetEntryName());
    if (it != entries.end() && it->second->type == lookup_info.GetCatalogType()) {
        return it->second.get();
    }
    
    return nullptr;
}

void MCPSchemaEntry::DropEntry(ClientContext &context, DropInfo &info) {
    lock_guard<mutex> lock(schema_lock);
    
    auto it = entries.find(info.name);
    if (it == entries.end()) {
        if (info.if_not_found == OnEntryNotFound::THROW_EXCEPTION) {
            throw CatalogException("Entry \"%s\" not found", info.name);
        }
        return;
    }
    
    entries.erase(it);
}

void MCPSchemaEntry::Alter(CatalogTransaction transaction, AlterInfo &info) {
    throw NotImplementedException("ALTER is not supported for MCP schemas - MCP resources are read-only");
}

} // namespace duckdb