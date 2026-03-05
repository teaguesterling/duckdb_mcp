#include "mcp_instance_state.hpp"
#include "duckdb/main/database.hpp"

namespace duckdb {

MCPInstanceState &MCPInstanceState::Get(DatabaseInstance &db) {
	auto &cache = db.GetObjectCache();
	auto state = cache.GetOrCreate<MCPInstanceState>(ObjectType());
	return *state;
}

MCPInstanceState &MCPInstanceState::Get(ClientContext &context) {
	return Get(DatabaseInstance::GetDatabase(context));
}

} // namespace duckdb
