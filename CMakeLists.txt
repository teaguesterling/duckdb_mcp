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
  src/mcp_instance_state.cpp
  src/duckdb_mcp_logging.cpp
  src/json_utils.cpp
  src/result_formatter.cpp
  src/protocol/mcp_message.cpp
  src/protocol/mcp_template.cpp
  src/protocol/mcp_pagination.cpp
  src/server/mcp_server.cpp
  src/server/mcp_state_table_functions.cpp
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

# WASM-only sources (WebMCP browser integration)
if(EMSCRIPTEN)
  list(APPEND EXTENSION_SOURCES
    src/server/webmcp_transport.cpp
  )
endif()

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

# Windows MSVC needs /std:c++17 to compile duckdb's vendored fmt/format.h
# (which uses C++17 inline variables). MSVC defaults to /std:c++14. duckdb's
# own Windows build must already use C++17 since it vendors fmt and links it
# into duckdb_static; aligning our extension targets here keeps the standard
# consistent across the link. Not applied on Linux: forcing C++17 on our
# extension there causes static-const data members in duckdb's headers to
# acquire implicit inline linkage in our TUs but not in libduckdb's
# C++11-built TUs, producing multiple-definition link errors.
if(MSVC)
    target_compile_options(${EXTENSION_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/std:c++17>")
    target_compile_options(${LOADABLE_EXTENSION_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/std:c++17>")
endif()

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
