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

# Thread safety tests (concurrent HTTP stress tests)
.PHONY: thread_safety_test thread_safety_test_release thread_safety_test_debug

thread_safety_test: thread_safety_test_release

thread_safety_test_release: release
	@echo "Running thread safety tests..."
	@chmod +x $(PROJ_DIR)test/integration/test_thread_safety.sh
	$(PROJ_DIR)test/integration/test_thread_safety.sh

thread_safety_test_debug: debug
	@echo "Running thread safety tests (debug build)..."
	@chmod +x $(PROJ_DIR)test/integration/test_thread_safety.sh
	BUILD_TYPE=debug $(PROJ_DIR)test/integration/test_thread_safety.sh

# Full test suite (SQL logic tests + integration tests)
.PHONY: test_all test_all_release

test_all: test_all_release

test_all_release: test_release integration_test_release
	@echo "All tests completed."

#===----------------------------------------------------------------------===#
# Formatting guards (issues #82, #85)
#===----------------------------------------------------------------------===#
# The format targets themselves come from extension-ci-tools. They are wrapped
# here rather than redefined: `format-check` etc. gain a prerequisite, not a new
# recipe, so upstream stays authoritative for what formatting means.
#
# The hazard being guarded is that clang-format does NOT error when its config
# is missing. It falls back to LLVM defaults and rewrites every file it touches,
# exit 0, nothing in the log. `.clang-format` used to be a symlink into the
# duckdb submodule, so that happened in any worktree or shallow clone (#85).
.PHONY: format-config-check format-safety-test

format-config-check:
	@if [ ! -e "$(PROJ_DIR).clang-format" ]; then \
		echo "ERROR: $(PROJ_DIR).clang-format does not resolve."; \
		echo "       clang-format would NOT error here -- it would silently fall back to"; \
		echo "       LLVM defaults and reformat every file wholesale (see issue #85)."; \
		echo "       Refusing to run the formatter."; \
		exit 1; \
	fi
	@command -v clang-format >/dev/null 2>&1 || { \
		echo "ERROR: clang-format not found on PATH."; \
		echo "       Install clang-format 11 (pip install 'clang_format==11.0.1')."; \
		exit 1; \
	}
	@col=`cd "$(PROJ_DIR)" && clang-format --dump-config 2>/dev/null | grep -E '^ColumnLimit:' | head -1 | awk '{print $$2}'`; \
	if [ "$$col" != "120" ]; then \
		echo "ERROR: effective clang-format style is not DuckDB's (ColumnLimit=$$col, expected 120)."; \
		echo "       Refusing to run the formatter -- see issue #85."; \
		exit 1; \
	fi
	@if [ -f "$(PROJ_DIR)duckdb/.clang-format" ]; then \
		if ! grep -v '^#' "$(PROJ_DIR).clang-format" | diff -q - "$(PROJ_DIR)duckdb/.clang-format" >/dev/null 2>&1; then \
			echo "NOTE: .clang-format has drifted from duckdb/.clang-format. If the submodule"; \
			echo "      bump was intentional, refresh the vendored copy (keeping its header)."; \
		fi; \
	fi

# Add the guard as a prerequisite of the upstream format targets.
format-check format format-fix format-main: format-config-check

# Regression tests for the two silent-corruption modes (#82 EM_JS, #85 config).
format-safety-test:
	@chmod +x $(PROJ_DIR)test/format/test_format_safety.sh
	$(PROJ_DIR)test/format/test_format_safety.sh

.PHONY: test-require
# A TEST RUN THAT SKIPPED REPORTS SUCCESS. `require duckdb_mcp` in a .test file means
# "skip unless available", so when the extension cannot be loaded EVERY file skips and
# the harness still prints "all tests passed" with exit 0. That is how 680 tests across
# this fleet went unrun behind 12 green registry checks in October 2026
# (duckdb/community-extensions#3086: the recorded extension version is truncated to 32
# bytes while the engine compares the untruncated value, so `require` finds nothing).
#
# `--require <name>` makes a MISSING require a hard failure instead -- see
# sqllogic_test_runner.cpp, where IsRequired(param) turns it into
# parser.Fail("require %s: FAILED"). It is SCOPED to the named requirement, so files
# gated on anything else (json, notwindows) still skip normally. That scoping is why this
# is used rather than run_tests.py's --fail-require-skip, which collapses every skip
# reason into one string and so cannot tell an unloadable extension from a deliberate
# platform gate -- and which is a no-op as shipped anyway, searching stdout while the
# summary goes to stderr (duckdb/duckdb#27413, with a verified fix).
#
# THE NAME IS WRITTEN OUT, not $(EXT_NAME), because the two are not always the same:
# sitting_duck sets EXT_NAME=duckdb_ast while its tests say `require sitting_duck`, so
# $(EXT_NAME) there would match no token and make this guard a silent no-op -- the exact
# failure class it exists to catch.
#
# WATCHED IT FAIL BOTH WAYS on DuckDB 2.0 (b15358c371d) before being trusted, by moving
# the built extension aside (measured on duck_block_utils):
#     make test-require, present : exit 0  "2 skipped, 813 assertions in 16 cases"
#     make test-require, hidden  : exit 2  "require duck_block_utils: FAILED"
#     make test      , hidden    : exit 0  "ran tests: 0 passed, 18 skipped"  <-- the bug
# The present case's legitimate skips passing is the proof the scoping works.
#
# INERT ON THE v1.5.6 LINE, where `require` cannot fail: hiding the artifact changes
# nothing (measured on markdown and yaml). Harmless there, load-bearing on 2.0.
# Report the ASSERTION count this prints, never `make test`'s file count.
test-require:
	./build/release/test/unittest --require duckdb_mcp "$(TESTS_BASE_DIRECTORY)*"
