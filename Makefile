PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=duckdb_mcp
EXT_CONFIG=${PROJ_DIR}extension_config.cmake

# Include the Makefile from extension-ci-tools
include extension-ci-tools/makefiles/duckdb_extension.Makefile

# Integration tests (run after build)
.PHONY: integration_test integration_test_release integration_test_debug

integration_test: integration_test_release

integration_test_release: release
	@echo "Running integration tests..."
	@chmod +x $(PROJ_DIR)test/integration/run_integration_tests.sh
	$(PROJ_DIR)test/integration/run_integration_tests.sh

integration_test_debug: debug
	@echo "Running integration tests (debug build)..."
	@chmod +x $(PROJ_DIR)test/integration/run_integration_tests.sh
	BUILD_TYPE=debug $(PROJ_DIR)test/integration/run_integration_tests.sh

# Full test suite (SQL logic tests + integration tests)
.PHONY: test_all test_all_release

test_all: test_all_release

test_all_release: test_release integration_test_release
	@echo "All tests completed."