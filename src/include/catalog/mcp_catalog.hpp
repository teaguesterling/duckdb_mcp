#pragma once

#include "duckdb.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_entry/schema_catalog_entry.hpp"
#include "duckdb/common/unordered_map.hpp"
#include "duckdb/common/mutex.hpp"
#include "catalog/mcp_schema_entry.hpp"

namespace duckdb {

class MCPConnection;

//! Minimal MCP catalog implementation for Phase 1
//! This catalog provides basic schema management and file access through MCPFS
//! Future phases will add table discovery and metadata integration
class MCPCatalog : public Catalog {
public:
    explicit MCPCatalog(AttachedDatabase &db, shared_ptr<MCPConnection> connection);
    ~MCPCatalog() override;

public:
    // Basic catalog info
    void Initialize(bool load_builtin) override;
    string GetCatalogType() override { return "mcp"; }
    bool InMemory() override { return true; }
    string GetDBPath() override { return ""; }

    // Schema management - required abstract methods
    optional_ptr<CatalogEntry> CreateSchema(CatalogTransaction transaction, CreateSchemaInfo &info) override;
    optional_ptr<SchemaCatalogEntry> LookupSchema(CatalogTransaction transaction,
                                                  const EntryLookupInfo &schema_lookup,
                                                  OnEntryNotFound if_not_found) override;
    void ScanSchemas(ClientContext &context, std::function<void(SchemaCatalogEntry &)> callback) override;
    void DropSchema(ClientContext &context, DropInfo &info) override;

    // Physical plan operations - minimal implementations for read-only access
    PhysicalOperator &PlanCreateTableAs(ClientContext &context, PhysicalPlanGenerator &planner,
                                       LogicalCreateTable &op, PhysicalOperator &plan) override;
    PhysicalOperator &PlanInsert(ClientContext &context, PhysicalPlanGenerator &planner, LogicalInsert &op,
                                optional_ptr<PhysicalOperator> plan) override;
    PhysicalOperator &PlanDelete(ClientContext &context, PhysicalPlanGenerator &planner, LogicalDelete &op,
                                PhysicalOperator &plan) override;
    PhysicalOperator &PlanUpdate(ClientContext &context, PhysicalPlanGenerator &planner, LogicalUpdate &op,
                                PhysicalOperator &plan) override;

    // Database metadata
    DatabaseSize GetDatabaseSize(ClientContext &context) override;

    // MCP-specific
    shared_ptr<MCPConnection> GetMCPConnection() const { return mcp_connection; }

private:
    // Create default schema if needed
    void CreateDefaultSchema();
    
    // Find or create a schema by name
    SchemaCatalogEntry &GetOrCreateSchema(const string &name);

private:
    shared_ptr<MCPConnection> mcp_connection;
    unordered_map<string, unique_ptr<MCPSchemaEntry>> schemas;
    mutex catalog_lock;
    bool initialized = false;
};

} // namespace duckdb