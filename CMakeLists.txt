cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME duckdb_mcp)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(src/include)

set(EXTENSION_SOURCES 
  src/duckdb_mcp_extension.cpp
  src/duckdb_mcp_config.cpp
  src/duckdb_mcp_security.cpp
  src/mcpfs/mcp_path.cpp
  src/mcpfs/mcp_file_system.cpp
  src/protocol/mcp_message.cpp
  src/protocol/mcp_transport.cpp
  src/protocol/mcp_connection.cpp
  src/client/mcp_storage_extension.cpp
  src/client/mcp_transaction_manager.cpp
  src/catalog/mcp_catalog.cpp
  src/catalog/mcp_schema_entry.cpp
)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
