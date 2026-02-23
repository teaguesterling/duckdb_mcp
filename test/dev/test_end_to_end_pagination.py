#!/usr/bin/env python3
"""
End-to-end test of MCP pagination functionality.
Tests DuckDB MCP extension with pagination-enabled MCP server.
"""

import asyncio
import subprocess
import sys
import os
import time
import logging
from pathlib import Path

# Set up logging
logging.basicConfig(level=logging.INFO, format='[E2E-TEST] %(asctime)s - %(message)s')
logger = logging.getLogger(__name__)


class E2EPaginationTest:
    def __init__(self):
        self.project_root = Path(__file__).parent
        self.duckdb_path = self.project_root / "build" / "release" / "duckdb"
        self.server_path = self.project_root / "test" / "fastmcp" / "pagination_test_server.py"
        self.venv_python = self.project_root / "venv" / "bin" / "python"
        self.server_process = None

    async def start_mcp_server(self):
        """Start the MCP pagination test server."""
        logger.info("Starting MCP pagination test server...")

        cmd = [str(self.venv_python), str(self.server_path)]

        self.server_process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=str(self.project_root),
        )

        logger.info(f"MCP server started with PID: {self.server_process.pid}")

        # Give server time to initialize
        await asyncio.sleep(2)

        if self.server_process.poll() is not None:
            stdout, stderr = self.server_process.communicate()
            logger.error(f"MCP server failed to start:")
            logger.error(f"STDOUT: {stdout}")
            logger.error(f"STDERR: {stderr}")
            raise RuntimeError("Failed to start MCP server")

        return self.server_process

    def stop_mcp_server(self):
        """Stop the MCP server."""
        if self.server_process:
            logger.info("Stopping MCP server...")
            self.server_process.terminate()
            try:
                self.server_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                logger.warning("Server didn't stop gracefully, forcing kill...")
                self.server_process.kill()
                self.server_process.wait()
            logger.info("MCP server stopped")

    def run_duckdb_test(self, sql_commands: list[str]) -> str:
        """Run DuckDB commands and return output."""
        logger.info(f"Running DuckDB test with {len(sql_commands)} commands...")

        # Combine SQL commands
        combined_sql = "; ".join(sql_commands)

        cmd = [str(self.duckdb_path), "-c", combined_sql]

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30, cwd=str(self.project_root))

            if result.returncode != 0:
                logger.error(f"DuckDB command failed with return code {result.returncode}")
                logger.error(f"STDERR: {result.stderr}")
                return f"ERROR: {result.stderr}"

            return result.stdout

        except subprocess.TimeoutExpired:
            logger.error("DuckDB command timed out")
            return "ERROR: Command timed out"
        except Exception as e:
            logger.error(f"Failed to run DuckDB command: {e}")
            return f"ERROR: {e}"

    def test_basic_connection(self):
        """Test basic MCP server connection."""
        logger.info("Testing basic MCP connection...")

        sql_commands = [
            "PRAGMA enable_mcp_extension",
            "SELECT mcp_server_start('pagination_server', 'stdio', '/home/teague/.local/bin/python', '-u', '/mnt/aux-data/teague/Projects/duckdb_mcp/test/fastmcp/pagination_test_server.py')",
            "SELECT mcp_server_status('pagination_server')",
        ]

        return self.run_duckdb_test(sql_commands)

    def test_pagination_functions(self):
        """Test pagination functions with real server."""
        logger.info("Testing pagination functions...")

        sql_commands = [
            "PRAGMA enable_mcp_extension",
            "SELECT mcp_server_start('pagination_server', 'stdio', '/mnt/aux-data/teague/Projects/duckdb_mcp/venv/bin/python', '-u', '/mnt/aux-data/teague/Projects/duckdb_mcp/test/fastmcp/pagination_test_server.py')",
            "SELECT mcp_list_resources_paginated('pagination_server', '') as page1",
            "SELECT mcp_list_prompts_paginated('pagination_server', '') as prompts_page1",
            "SELECT mcp_list_tools_paginated('pagination_server', '') as tools_page1",
        ]

        return self.run_duckdb_test(sql_commands)

    def test_cursor_navigation(self):
        """Test cursor-based pagination navigation."""
        logger.info("Testing cursor navigation...")

        sql_commands = [
            "PRAGMA enable_mcp_extension",
            "SELECT mcp_server_start('pagination_server', 'stdio', '/mnt/aux-data/teague/Projects/duckdb_mcp/venv/bin/python', '-u', '/mnt/aux-data/teague/Projects/duckdb_mcp/test/fastmcp/pagination_test_server.py')",
            # Get first page
            "WITH page1 AS (SELECT mcp_list_resources_paginated('pagination_server', '') as result) SELECT result.next_cursor FROM page1",
            # Use cursor for second page
            "WITH page1 AS (SELECT mcp_list_resources_paginated('pagination_server', '') as result), page2 AS (SELECT mcp_list_resources_paginated('pagination_server', page1.result.next_cursor) as result FROM page1) SELECT page2.result.has_more_pages FROM page2",
        ]

        return self.run_duckdb_test(sql_commands)

    async def run_all_tests(self):
        """Run all end-to-end tests."""
        logger.info("Starting end-to-end pagination tests...")

        tests = [
            ("Basic Connection Test", self.test_basic_connection),
            ("Pagination Functions Test", self.test_pagination_functions),
            ("Cursor Navigation Test", self.test_cursor_navigation),
        ]

        results = {}

        for test_name, test_func in tests:
            logger.info(f"Running: {test_name}")
            try:
                result = test_func()
                results[test_name] = result
                logger.info(f"✅ {test_name} completed")
                if "ERROR" not in result:
                    logger.info(f"Result preview: {result[:200]}...")
                else:
                    logger.error(f"❌ {test_name} failed: {result}")
            except Exception as e:
                results[test_name] = f"EXCEPTION: {e}"
                logger.error(f"❌ {test_name} threw exception: {e}")

        return results


def main():
    test_runner = E2EPaginationTest()

    try:
        # Run the tests
        results = asyncio.run(test_runner.run_all_tests())

        # Print summary
        print("\n" + "=" * 60)
        print("END-TO-END PAGINATION TEST RESULTS")
        print("=" * 60)

        for test_name, result in results.items():
            status = "✅ PASS" if "ERROR" not in result and "EXCEPTION" not in result else "❌ FAIL"
            print(f"{status} {test_name}")
            if len(result) > 500:
                print(f"   Result: {result[:500]}...")
            else:
                print(f"   Result: {result}")
            print()

        # Overall status
        failed_tests = [name for name, result in results.items() if "ERROR" in result or "EXCEPTION" in result]
        if failed_tests:
            print(f"❌ {len(failed_tests)} tests failed: {', '.join(failed_tests)}")
            return 1
        else:
            print(f"✅ All {len(results)} tests passed!")
            return 0

    except KeyboardInterrupt:
        logger.info("Test interrupted by user")
        return 1
    except Exception as e:
        logger.error(f"Test runner failed: {e}")
        return 1
    finally:
        test_runner.stop_mcp_server()


if __name__ == "__main__":
    sys.exit(main())
