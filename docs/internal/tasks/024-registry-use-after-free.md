# Task: Fix Use-After-Free in Registry Pointer Access

**GitHub Issue:** #24
**Finding:** CR-04
**Severity:** High
**Files:** `src/server/mcp_server.cpp`, `src/include/server/mcp_server.hpp`

## Problem

`ResourceRegistry::GetResource()` and `ToolRegistry::GetTool()` return raw
pointers to `unique_ptr`-owned objects after releasing the registry mutex.
Callers dereference these pointers without holding any lock. If another thread
unregisters the entry between return and dereference, the pointer is dangling.

### Current Code

```cpp
// mcp_server.cpp:45-49
ResourceProvider *ResourceRegistry::GetResource(const string &uri) const {
    lock_guard<mutex> lock(registry_mutex);
    auto it = resources.find(uri);
    return it != resources.end() ? it->second.get() : nullptr;
}
```

### Affected Callers

- `HandleResourcesList` (line 553) — iterates resources and calls `GetResource`
  then dereferences: `provider->GetDescription()`, `provider->GetMimeType()`
- `HandleResourcesRead` (line 606) — calls `GetResource` then `provider->Read()`
- `HandleToolsList` (line 643) — calls `GetTool` then `handler->GetDescription()`,
  `handler->GetInputSchema()`
- `HandleToolsCall` (line 715) — calls `GetTool` then `handler->Execute()`

## Recommended Fix: Switch to shared_ptr

The simplest fix with the least API disruption:

### 1. Change registry storage to `shared_ptr`

In `src/include/server/mcp_server.hpp`:

```cpp
// ResourceRegistry
unordered_map<string, shared_ptr<ResourceProvider>> resources;

// ToolRegistry
unordered_map<string, shared_ptr<ToolHandler>> tools;
```

### 2. Update RegisterResource/RegisterTool to accept shared_ptr

```cpp
void RegisterResource(const string &uri, shared_ptr<ResourceProvider> provider);
void RegisterTool(const string &name, shared_ptr<ToolHandler> handler);
```

### 3. Return shared_ptr from GetResource/GetTool

```cpp
shared_ptr<ResourceProvider> GetResource(const string &uri) const;
shared_ptr<ToolHandler> GetTool(const string &name) const;
```

### 4. Update callers

All callers of `PublishResource` and `RegisterTool` currently pass `unique_ptr`.
They need to be changed to `shared_ptr` (or use `make_shared`). Key locations:

- `MCPServer::RegisterBuiltinTools()` (mcp_server.cpp:747-783) — change
  `make_uniq` to `make_shared`
- `MCPServer::PublishResource()` (mcp_server.cpp:274)
- `MCPServer::RegisterTool()` (mcp_server.cpp:288)
- `ApplyPendingRegistrations()` (mcp_server.cpp:879-921)
- `ApplyPendingRegistrationsTo()` (mcp_server.cpp:923-961)
- Various publish/register calls in `duckdb_mcp_extension.cpp`

### 5. Update handler callers to hold the shared_ptr

```cpp
// Before:
auto handler = tool_registry.GetTool(tool_name);
if (!handler) { ... }
auto call_result = handler->Execute(arguments);

// After (same pattern, but handler is now shared_ptr keeping object alive):
auto handler = tool_registry.GetTool(tool_name);
if (!handler) { ... }
auto call_result = handler->Execute(arguments);
```

## Alternative Fix: Hold Lock During Operation

Less disruptive but coarser-grained: add methods that execute a callback under
the registry lock. This avoids changing the storage type but increases lock
contention.

## Verification

1. `make test` passes
2. `make integration_test` passes
3. Verify tools/list, tools/call, resources/list, resources/read all work
   via memory transport tests

## Scope

- `src/include/server/mcp_server.hpp` — registry class definitions
- `src/server/mcp_server.cpp` — registry implementations and callers
- `src/duckdb_mcp_extension.cpp` — publish/register call sites
