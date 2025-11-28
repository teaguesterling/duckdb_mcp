# Development Tests

Ad-hoc development and debugging scripts for testing MCP functionality. These are **not** part of the formal test suite run by `make test`.

## Contents

- `test_basic_mcp.sql` - Basic MCP connectivity test
- `test_end_to_end_pagination.py` - End-to-end pagination testing
- `test_pagination_*.sql` - Various pagination scenario tests
- `test_template*.sql` - Prompt template testing
- `test_logging.sql` - Logging functionality tests

## Usage

These scripts may contain hardcoded paths and require manual setup. They're preserved for reference and debugging purposes.

For formal testing, use:
```bash
make test
```

Or run the Python integration tests:
```bash
python test/test_documented_features.py
python test/test_security_validation.py
```
