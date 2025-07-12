#pragma once

#include "duckdb.hpp"
#include "duckdb/catalog/catalog_entry/schema_catalog_entry.hpp"
#include "duckdb/common/unordered_map.hpp"
#include "duckdb/common/mutex.hpp"

namespace duckdb {

class MCPConnection;

//! Minimal MCP schema implementation for Phase 1
//! This schema provides basic table storage and lookup for MCPFS
//! Future phases will add table discovery and metadata integration
class MCPSchemaEntry : public SchemaCatalogEntry {
public:
    explicit MCPSchemaEntry(Catalog &catalog, CreateSchemaInfo &info, shared_ptr<MCPConnection> connection = nullptr);
    ~MCPSchemaEntry() override;

public:
    // Core schema operations
    void Scan(ClientContext &context, CatalogType type, const std::function<void(CatalogEntry &)> &callback) override;
    void Scan(CatalogType type, const std::function<void(CatalogEntry &)> &callback) override;

    // Entry creation - most not supported in Phase 1 MCP
    optional_ptr<CatalogEntry> CreateIndex(CatalogTransaction transaction, CreateIndexInfo &info,
                                          TableCatalogEntry &table) override;
    optional_ptr<CatalogEntry> CreateFunction(CatalogTransaction transaction, CreateFunctionInfo &info) override;
    optional_ptr<CatalogEntry> CreateTable(CatalogTransaction transaction, BoundCreateTableInfo &info) override;
    optional_ptr<CatalogEntry> CreateView(CatalogTransaction transaction, CreateViewInfo &info) override;
    optional_ptr<CatalogEntry> CreateSequence(CatalogTransaction transaction, CreateSequenceInfo &info) override;
    optional_ptr<CatalogEntry> CreateTableFunction(CatalogTransaction transaction,
                                                   CreateTableFunctionInfo &info) override;
    optional_ptr<CatalogEntry> CreateCopyFunction(CatalogTransaction transaction,
                                                  CreateCopyFunctionInfo &info) override;
    optional_ptr<CatalogEntry> CreatePragmaFunction(CatalogTransaction transaction,
                                                    CreatePragmaFunctionInfo &info) override;
    optional_ptr<CatalogEntry> CreateCollation(CatalogTransaction transaction, CreateCollationInfo &info) override;
    optional_ptr<CatalogEntry> CreateType(CatalogTransaction transaction, CreateTypeInfo &info) override;

    // Entry lookup and management
    optional_ptr<CatalogEntry> LookupEntry(CatalogTransaction transaction,
                                          const EntryLookupInfo &lookup_info) override;
    void DropEntry(ClientContext &context, DropInfo &info) override;
    void Alter(CatalogTransaction transaction, AlterInfo &info) override;

    // MCP-specific
    shared_ptr<MCPConnection> GetMCPConnection() const { return mcp_connection; }

private:
    shared_ptr<MCPConnection> mcp_connection;
    unordered_map<string, unique_ptr<CatalogEntry>> entries;
    mutex schema_lock;
};

} // namespace duckdb