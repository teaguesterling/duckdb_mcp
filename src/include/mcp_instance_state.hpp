#pragma once

#include "duckdb.hpp"
#include "duckdb/storage/object_cache.hpp"
#include "duckdb_mcp_config.hpp"
#include "duckdb_mcp_security.hpp"
#include "protocol/mcp_template.hpp"
#include "server/mcp_server.hpp"
#ifndef __EMSCRIPTEN__
#include "client/mcp_storage_extension.hpp"
#endif

namespace duckdb {

//! Per-DatabaseInstance state for the MCP extension.
//! Stored in the ObjectCache so it is automatically cleaned up when
//! the DatabaseInstance is destroyed.
class MCPInstanceState : public ObjectCacheEntry {
public:
	static string ObjectType() {
		return "mcp_instance_state";
	}
	string GetObjectType() override {
		return ObjectType();
	}
	//! Return invalid to prevent eviction
	optional_idx GetEstimatedCacheMemory() const override {
		return optional_idx();
	}

	//! Get the per-instance state for a DatabaseInstance
	static MCPInstanceState &Get(DatabaseInstance &db);
	//! Get the per-instance state from a ClientContext
	static MCPInstanceState &Get(ClientContext &context);

	//! Per-instance MCP configuration
	MCPConfiguration config;

	//! Per-instance security configuration
	MCPSecurityConfig security;

	//! Per-instance template manager
	MCPTemplateManager template_manager;

	//! Per-instance server manager
	MCPServerManager server_manager;

#ifndef __EMSCRIPTEN__
	//! Per-instance connection registry
	MCPConnectionRegistry connection_registry;
#endif
};

} // namespace duckdb
