# Testing this extension
This directory contains all the tests for this extension. The `sql` directory holds tests that are written as [SQLLogicTests](https://duckdb.org/dev/sqllogictest/intro.html). DuckDB aims to have most its tests in this format as SQL statements, so for the quack extension, this should probably be the goal too.

The root makefile contains targets to build and run all of these tests. To run the SQLLogicTests:
```bash
make test
```
or 
```bash
make test_debug
```
## Formatting safety tests (`test/format/`)

`test/format/test_format_safety.sh` guards two ways this repo's formatting
tooling used to damage source *without saying so* — in both, clang-format
exits 0:

- **#82** — clang-format formats `EM_JS` macro arguments as C++, rewriting the
  JavaScript strict-inequality operator `!==` to `!= =`. Those bodies compile
  into the WASM build only, so native builds and native tests cannot see the
  break.
- **#85** — `.clang-format` used to be a symlink into the `duckdb` submodule.
  Wherever submodules are not initialised (worktrees, shallow clones) it
  dangled, and clang-format silently fell back to LLVM defaults, reformatting
  whole files.

```bash
make format-safety-test          # or: ./test/format/test_format_safety.sh
```

Requires clang-format 11 (`pip install 'clang_format==11.0.1'`). It is a hard
requirement rather than a skip: skipping when the tool is missing is the same
silent-pass failure mode the script exists to catch. It runs in CI as the
`format-safety` job, deliberately without submodules checked out.

The `make format-*` targets additionally refuse to run when the style config
does not resolve or is not DuckDB's — see `format-config-check` in the Makefile.
