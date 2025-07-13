#pragma once

#include "duckdb.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/common/optional_ptr.hpp"

namespace duckdb {

// Result structure for resource reads
struct ReadResourceResult {
    bool success = false;
    string content;
    string mime_type;
    string error_message;
    
    static ReadResourceResult Success(const string &content, const string &mime_type = "text/plain") {
        ReadResourceResult result;
        result.success = true;
        result.content = content;
        result.mime_type = mime_type;
        return result;
    }
    
    static ReadResourceResult Error(const string &error) {
        ReadResourceResult result;
        result.success = false;
        result.error_message = error;
        return result;
    }
};

// Abstract base class for resource providers
class ResourceProvider {
public:
    virtual ~ResourceProvider() = default;
    
    // Read resource content
    virtual ReadResourceResult Read() = 0;
    
    // Get resource metadata
    virtual string GetMimeType() const = 0;
    virtual size_t GetSize() const = 0;
    virtual string GetDescription() const = 0;
    
    // Check if resource supports refresh
    virtual bool IsRefreshable() const { return false; }
    virtual bool ShouldRefresh() const { return false; }
    virtual void Refresh() {}
};

// Table resource provider - publishes DuckDB table as MCP resource
class TableResourceProvider : public ResourceProvider {
public:
    TableResourceProvider(const string &table_name, const string &format, DatabaseInstance &db);
    
    ReadResourceResult Read() override;
    string GetMimeType() const override;
    size_t GetSize() const override;
    string GetDescription() const override;

private:
    string table_name;
    string format; // "json", "csv", "arrow"
    DatabaseInstance &db_instance;
    
    string FormatResult(QueryResult &result) const;
};

// Query resource provider - publishes query results as MCP resource
class QueryResourceProvider : public ResourceProvider {
public:
    QueryResourceProvider(const string &query, const string &format, 
                         DatabaseInstance &db, uint32_t refresh_interval_seconds = 0);
    
    ReadResourceResult Read() override;
    string GetMimeType() const override;
    size_t GetSize() const override;
    string GetDescription() const override;
    
    bool IsRefreshable() const override { return refresh_interval_seconds > 0; }
    bool ShouldRefresh() const override;
    void Refresh() override;

private:
    string query;
    string format; // "json", "csv", "arrow"
    DatabaseInstance &db_instance;
    uint32_t refresh_interval_seconds;
    time_t last_refresh_time;
    string cached_content;
    bool content_cached;
    mutable mutex cache_mutex;
    
    string ExecuteQuery() const;
    string FormatResult(QueryResult &result) const;
};

// Static content resource provider - for fixed content
class StaticResourceProvider : public ResourceProvider {
public:
    StaticResourceProvider(const string &content, const string &mime_type, const string &description);
    
    ReadResourceResult Read() override;
    string GetMimeType() const override;
    size_t GetSize() const override;
    string GetDescription() const override;

private:
    string content;
    string mime_type;
    string description;
};

} // namespace duckdb