#!/bin/bash
# Regression test for the two ways this repo's formatting tooling used to damage
# source without saying so. Both share the dangerous property that *the tool
# succeeds*: exit 0, nothing in the log, and a large plausible-looking diff.
#
#   #82  clang-format reformats the JavaScript inside EM_JS macros as C++.
#        `!==` is not a C++ operator, so it is reparsed as `!=` followed by `=`
#        and respaced to `!= =` — a JavaScript syntax error. Those bodies are
#        compiled into the WASM build ONLY, so native builds and the native test
#        suite cannot see the break.
#
#   #85  `.clang-format` used to be a symlink into the `duckdb` submodule. In a
#        `git worktree add`, a shallow CI clone, or a fresh clone before
#        `git submodule update`, the link dangles — and clang-format does not
#        error on a missing config. It silently falls back to LLVM defaults
#        (2-space / 80 col) and reformats whole files.
#
# This script asserts the fixes for both, plus that the tree as a whole is
# format-clean, so that a future `make format-fix` is a no-op diff and any
# reintroduction of the EM_JS corruption is impossible to miss in review.
#
# Requires clang-format (DuckDB uses 11.0.1). It is a hard requirement, not a
# skip: silently skipping is the same failure mode the test exists to catch.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

EM_JS_FILE="src/server/webmcp_transport.cpp"

FAILURES=0
WARNINGS=0

pass() { echo "  PASS: $1"; }
fail() { echo "  FAIL: $1"; FAILURES=$((FAILURES + 1)); }
warn() { echo "  WARN: $1"; WARNINGS=$((WARNINGS + 1)); }

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
	echo "ERROR: $CLANG_FORMAT not found. Install clang-format 11 (e.g. pip install 'clang_format==11.0.1')"
	echo "       or set CLANG_FORMAT=/path/to/clang-format."
	exit 1
fi

cd "$PROJECT_DIR" || exit 1

echo "=========================================="
echo "Formatting safety tests (#82, #85)"
echo "=========================================="
echo "clang-format: $("$CLANG_FORMAT" --version)"
echo "project:      $PROJECT_DIR"
echo

# The scratch dir MUST live inside the project. clang-format finds its style by
# walking up the parent directories of the file being formatted, so a copy in
# /tmp would be formatted with LLVM defaults and every comparison below would be
# measuring the wrong thing.
TMPDIR_TEST="$(mktemp -d "$PROJECT_DIR/.format-safety-test.XXXXXX")"
cleanup() { rm -rf "$TMPDIR_TEST"; }
trap cleanup EXIT

#-----------------------------------------------------------------------------
# 1. (#85) The style config must actually resolve, and must be DuckDB's style.
#-----------------------------------------------------------------------------
echo "[1] .clang-format resolves to a real DuckDB-style config"

if [ ! -e .clang-format ]; then
	fail ".clang-format does not resolve (missing, or a dangling symlink)."
	echo "       clang-format would silently fall back to LLVM defaults here."
elif [ -L .clang-format ]; then
	# A resolving symlink is not fatal, but it is how #85 happened: the target
	# was inside the duckdb submodule, which is absent in worktrees/shallow CI.
	warn ".clang-format is a symlink ($(readlink .clang-format)). See issue #85 —"
	echo "        a link into a submodule dangles in worktrees and shallow clones."
else
	pass ".clang-format is a regular file"
fi

# The load-bearing assertion is not "a file exists" but "DuckDB's style is in
# effect". LLVM defaults are ColumnLimit 80 / UseTab Never; DuckDB is 120 /
# ForIndentation. This catches a fallback no matter how it comes about.
DUMPED="$("$CLANG_FORMAT" --dump-config 2>/dev/null)"
COL="$(echo "$DUMPED" | grep -E '^ColumnLimit:' | head -1 | awk '{print $2}')"
TABS="$(echo "$DUMPED" | grep -E '^UseTab:' | head -1 | awk '{print $2}')"
if [ "$COL" = "120" ] && [ "$TABS" = "ForIndentation" ]; then
	pass "effective style is DuckDB's (ColumnLimit=120, UseTab=ForIndentation)"
else
	fail "effective style is NOT DuckDB's (ColumnLimit=$COL, UseTab=$TABS)"
	echo "       clang-format has fallen back to defaults — every file it touches"
	echo "       will be rewritten wholesale."
fi
echo

#-----------------------------------------------------------------------------
# 2. (#82) Negative control: prove the corruption is real without the guard.
#-----------------------------------------------------------------------------
# Without this, test 3 would pass vacuously the day clang-format stops mangling
# EM_JS — and we would not notice that the guard had become decorative.
echo "[2] negative control: corruption reproduces when the guard is removed"

UNGUARDED="$TMPDIR_TEST/unguarded.cpp"
grep -v -e '^// clang-format off$' -e '^// clang-format on$' "$EM_JS_FILE" >"$UNGUARDED"
"$CLANG_FORMAT" -i "$UNGUARDED"
CTRL_HITS="$(grep -c '!= =' "$UNGUARDED" || true)"
# The guard's own explanatory comment mentions the broken operator; drop those.
CTRL_CODE_HITS="$(grep '!= =' "$UNGUARDED" | grep -cv '^\s*//' || true)"
if [ "$CTRL_CODE_HITS" -gt 0 ]; then
	pass "without the guard, clang-format produces $CTRL_CODE_HITS occurrence(s) of '!= =' (exit 0, no error)"
else
	warn "the '!== -> != =' corruption no longer reproduces with this clang-format."
	echo "        Test 3 below is therefore not proving anything. Re-check issue #82"
	echo "        before concluding the guard can be dropped."
fi
echo

#-----------------------------------------------------------------------------
# 3. (#82) The guard is present and the EM_JS JavaScript survives formatting.
#-----------------------------------------------------------------------------
echo "[3] EM_JS bodies survive clang-format unchanged"

if grep -q '^// clang-format off$' "$EM_JS_FILE" && grep -q '^// clang-format on$' "$EM_JS_FILE"; then
	pass "clang-format off/on guard present in $EM_JS_FILE"
else
	fail "clang-format off/on guard missing from $EM_JS_FILE"
fi

GUARDED="$TMPDIR_TEST/guarded.cpp"
cp "$EM_JS_FILE" "$GUARDED"
"$CLANG_FORMAT" -i "$GUARDED"
if cmp -s "$GUARDED" "$EM_JS_FILE"; then
	pass "clang-format is a no-op on $EM_JS_FILE"
else
	fail "clang-format modifies $EM_JS_FILE"
	diff -u "$EM_JS_FILE" "$GUARDED" | head -40
fi

# Assert on the operator directly, so this still means something even if the
# whole-file comparison above is ever relaxed.
STRICT_BEFORE="$(grep -c '!==' "$EM_JS_FILE" || true)"
STRICT_AFTER="$(grep -c '!==' "$GUARDED" || true)"
BROKEN_AFTER="$(grep '!= =' "$GUARDED" | grep -cv '^\s*//' || true)"
if [ "$STRICT_BEFORE" -eq 0 ]; then
	warn "no '!==' left in $EM_JS_FILE — this test's subject has moved"
elif [ "$STRICT_AFTER" -eq "$STRICT_BEFORE" ] && [ "$BROKEN_AFTER" -eq 0 ]; then
	pass "all $STRICT_BEFORE strict-inequality operators intact, no '!= =' introduced"
else
	fail "strict-inequality damaged: $STRICT_BEFORE '!==' before, $STRICT_AFTER after, $BROKEN_AFTER '!= =' introduced"
fi
echo

#-----------------------------------------------------------------------------
# 4. (#82, follow-on) The tree is format-clean, so format-fix is a no-op diff.
#-----------------------------------------------------------------------------
# A large formatting diff is exactly what hides a corruption like #82. Keeping
# the tree at zero drift is what makes the next one visible.
echo "[4] src/ and test/ are already format-clean"

DRIFTED=0
while IFS= read -r f; do
	FORMATTED="$TMPDIR_TEST/fmt.cpp"
	"$CLANG_FORMAT" "$f" >"$FORMATTED" 2>/dev/null
	if ! cmp -s "$FORMATTED" "$f"; then
		echo "       drift: $f ($(diff "$f" "$FORMATTED" | grep -c '^[<>]') lines)"
		DRIFTED=$((DRIFTED + 1))
	fi
done < <(find src test -type f \( -name '*.cpp' -o -name '*.hpp' \) | sort)

if [ "$DRIFTED" -eq 0 ]; then
	pass "no formatting drift in src/ or test/"
else
	fail "$DRIFTED file(s) drift from the configured style — run 'make format-fix'"
fi
echo

echo "=========================================="
if [ "$FAILURES" -eq 0 ]; then
	echo "All formatting safety tests PASSED ($WARNINGS warning(s))"
	exit 0
fi
echo "$FAILURES formatting safety test(s) FAILED ($WARNINGS warning(s))"
exit 1
