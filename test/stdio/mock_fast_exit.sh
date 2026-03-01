#!/bin/bash
# Mock MCP server that exits immediately after starting.
# Used to test CR-09 (PID reuse race) and CR-21 (fast failure detection).
exit 0
