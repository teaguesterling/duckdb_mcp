#!/bin/bash
# MCP Server Launcher Script
# This script launches the FastMCP sample data server with the correct environment

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Set working directory to project root
cd "$SCRIPT_DIR"

# Use the venv Python to run the server
exec ./venv/bin/python test/fastmcp/sample_data_server.py