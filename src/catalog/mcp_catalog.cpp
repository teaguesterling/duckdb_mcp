#include "catalog/mcp_catalog.hpp"
#include "catalog/mcp_schema_entry.hpp"
#include "protocol/mcp_connection.hpp"
#include "duckdb/catalog/catalog_entry/schema_catalog_entry.hpp"
#include "duckdb/parser/parsed_data/create_schema_info.hpp"
#include "duckdb/parser/parsed_data/drop_info.hpp"
#include "duckdb/planner/logical_operator.hpp"
#include "duckdb/execution/physical_plan_generator.hpp"
#include "duckdb/execution/operator/persistent/physical_insert.hpp"
#include "duckdb/execution/operator/persistent/physical_delete.hpp"
#include "duckdb/execution/operator/persistent/physical_update.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/catalog/default/default_schemas.hpp"
#include "duckdb/storage/database_size.hpp"

namespace duckdb {

MCPCatalog::MCPCatalog(AttachedDatabase &db, shared_ptr<MCPConnection> connection)
    : Catalog(db), mcp_connection(connection) {
}

MCPCatalog::~MCPCatalog() = default;

void MCPCatalog::Initialize(bool load_builtin) {
    lock_guard<mutex> lock(catalog_lock);
    if (initialized) {
        return;
    }
    
    // Create default "main" schema for MCP resources
    CreateDefaultSchema();
    initialized = true;
}

void MCPCatalog::CreateDefaultSchema() {
    // Create the default "main" schema
    CreateSchemaInfo schema_info;
    schema_info.schema = "main";
    schema_info.on_conflict = OnCreateConflict::IGNORE_ON_CONFLICT;
    
    auto schema_entry = make_uniq<MCPSchemaEntry>(*this, schema_info, mcp_connection);
    schemas["main"] = std::move(schema_entry);
}

optional_ptr<CatalogEntry> MCPCatalog::CreateSchema(CatalogTransaction transaction, CreateSchemaInfo &info) {
    lock_guard<mutex> lock(catalog_lock);
    
    if (schemas.find(info.schema) != schemas.end()) {
        if (info.on_conflict == OnCreateConflict::ERROR_ON_CONFLICT) {
            throw CatalogException("Schema \"%s\" already exists", info.schema);
        }
        // Schema already exists, return existing entry
        return schemas[info.schema].get();
    }
    
    // Create new schema
    auto schema_entry = make_uniq<MCPSchemaEntry>(*this, info, mcp_connection);
    auto result = schema_entry.get();
    schemas[info.schema] = std::move(schema_entry);
    return result;
}

optional_ptr<SchemaCatalogEntry> MCPCatalog::LookupSchema(CatalogTransaction transaction,
                                                         const EntryLookupInfo &schema_lookup,
                                                         OnEntryNotFound if_not_found) {
    lock_guard<mutex> lock(catalog_lock);
    
    auto it = schemas.find(schema_lookup.GetEntryName());
    if (it != schemas.end()) {
        return it->second.get();
    }
    
    if (if_not_found == OnEntryNotFound::THROW_EXCEPTION) {
        throw CatalogException("Schema \"%s\" not found", schema_lookup.GetEntryName());
    }
    
    return nullptr;
}

void MCPCatalog::ScanSchemas(ClientContext &context, std::function<void(SchemaCatalogEntry &)> callback) {
    lock_guard<mutex> lock(catalog_lock);
    
    for (auto &schema_pair : schemas) {
        callback(*schema_pair.second);
    }
}

void MCPCatalog::DropSchema(ClientContext &context, DropInfo &info) {
    lock_guard<mutex> lock(catalog_lock);
    
    auto it = schemas.find(info.name);
    if (it == schemas.end()) {
        if (info.if_not_found == OnEntryNotFound::THROW_EXCEPTION) {
            throw CatalogException("Schema \"%s\" not found", info.name);
        }
        return;
    }
    
    // Don't allow dropping the main schema
    if (info.name == "main") {
        throw CatalogException("Cannot drop the main schema");
    }
    
    schemas.erase(it);
}

SchemaCatalogEntry &MCPCatalog::GetOrCreateSchema(const string &name) {
    lock_guard<mutex> lock(catalog_lock);
    
    auto it = schemas.find(name);
    if (it != schemas.end()) {
        return *it->second;
    }
    
    // Create new schema
    CreateSchemaInfo schema_info;
    schema_info.schema = name;
    schema_info.on_conflict = OnCreateConflict::IGNORE_ON_CONFLICT;
    
    auto schema_entry = make_uniq<MCPSchemaEntry>(*this, schema_info, mcp_connection);
    auto result = schema_entry.get();
    schemas[name] = std::move(schema_entry);
    return *result;
}

// Physical plan operations - MCP is read-only for Phase 1
PhysicalOperator &MCPCatalog::PlanCreateTableAs(ClientContext &context, PhysicalPlanGenerator &planner,
                                               LogicalCreateTable &op, PhysicalOperator &plan) {
    throw NotImplementedException("CREATE TABLE AS is not supported for MCP catalogs in Phase 1");
}

PhysicalOperator &MCPCatalog::PlanInsert(ClientContext &context, PhysicalPlanGenerator &planner, LogicalInsert &op,
                                        optional_ptr<PhysicalOperator> plan) {
    throw NotImplementedException("INSERT is not supported for MCP catalogs - MCP resources are read-only");
}

PhysicalOperator &MCPCatalog::PlanDelete(ClientContext &context, PhysicalPlanGenerator &planner, LogicalDelete &op,
                                        PhysicalOperator &plan) {
    throw NotImplementedException("DELETE is not supported for MCP catalogs - MCP resources are read-only");
}

PhysicalOperator &MCPCatalog::PlanUpdate(ClientContext &context, PhysicalPlanGenerator &planner, LogicalUpdate &op,
                                        PhysicalOperator &plan) {
    throw NotImplementedException("UPDATE is not supported for MCP catalogs - MCP resources are read-only");
}

DatabaseSize MCPCatalog::GetDatabaseSize(ClientContext &context) {
    // MCP databases are virtual, return minimal size info
    DatabaseSize size;
    size.total_blocks = 0;
    size.block_size = 0;
    size.free_blocks = 0;
    size.used_blocks = 0;
    size.bytes = 0;
    size.wal_size = 0;
    return size;
}

} // namespace duckdb