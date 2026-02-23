cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME duckdb_mcp)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(src/include)
if(NOT EMSCRIPTEN)
  include_directories(${CMAKE_SOURCE_DIR}/third_party/httplib)
endif()

set(EXTENSION_SOURCES
  src/duckdb_mcp_extension.cpp
  src/duckdb_mcp_config.cpp
  src/duckdb_mcp_security.cpp
  src/duckdb_mcp_logging.cpp
  src/json_utils.cpp
  src/result_formatter.cpp
  src/protocol/mcp_message.cpp
  src/protocol/mcp_template.cpp
  src/protocol/mcp_pagination.cpp
  src/server/mcp_server.cpp
  src/server/resource_providers.cpp
  src/server/tool_handlers.cpp
  src/server/memory_transport.cpp
)

# Sources that are incompatible with WASM (process management, HTTP server,
# file I/O, client-side ATTACH, threading)
if(NOT EMSCRIPTEN)
  list(APPEND EXTENSION_SOURCES
    src/protocol/mcp_transport.cpp
    src/protocol/mcp_connection.cpp
    src/protocol/mcp_http_transport.cpp
    src/server/http_server_transport.cpp
    src/server/stdio_server_transport.cpp
    src/client/mcp_storage_extension.cpp
    src/client/mcp_transaction_manager.cpp
    src/catalog/mcp_catalog.cpp
    src/catalog/mcp_schema_entry.cpp
    src/mcpfs/mcp_path.cpp
    src/mcpfs/mcp_file_system.cpp
  )
endif()

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
