#pragma once

#include "duckdb.hpp"

// duckdb_compat.hpp — fleet-standard cross-version shim for DuckDB extensions.
//
// Pattern established by @bendrucker in teaguesterling/duckdb_webbed#76 (May 2026):
// detect the new API via __has_include of headers that moved in the same DuckDB
// refactor ([duckdb/duckdb#22377](https://github.com/duckdb/duckdb/pull/22377) —
// "mandatory per-vector size tracking" landed alongside the vector-buffer header
// reshuffle), then dispatch via a single #ifdef block.
//
// Cross-version coverage:
//   - duckdb v1.4.x / v1.5.x: old API everywhere
//   - duckdb main / v1.6.x:   new API everywhere
//
// See teaguesterling/duckdb_markdown's docs/DUCKDB_API_MIGRATION.md for the
// long-form rationale + upgrade checklist for other extensions.

#if __has_include("duckdb/common/vector/list_vector.hpp")
#define DUCKDB_HAS_NEW_VECTOR_HEADERS 1
#include "duckdb/common/vector/list_vector.hpp"
#include "duckdb/common/vector/struct_vector.hpp"
#endif

namespace duckdb {

#ifdef DUCKDB_HAS_NEW_VECTOR_HEADERS

// --- Output chunk finalization ---
// DuckDB main mandates per-vector Size() tracking; DataChunk::SetCardinality only
// updates chunk.count. SetChildCardinality additionally calls FlatVector::SetSize
// on every column so query operators reading vec.Size() see the right value.
// Without this, VariadicExecutor (and similar) reports:
//   "Mismatch in input vector sizes ... expected 0 rows but got N"
inline void CompatSetOutputCardinality(DataChunk &chunk, idx_t count) {
	chunk.SetChildCardinality(count);
}

// --- ScalarFunction property setters (fields are now private) ---
inline void SetScalarFunctionNullHandling(ScalarFunction &func, FunctionNullHandling handling) {
	func.SetNullHandling(handling);
}

#else // Old API (v1.4.x / v1.5.x)

inline void CompatSetOutputCardinality(DataChunk &chunk, idx_t count) {
	chunk.SetCardinality(count);
}

inline void SetScalarFunctionNullHandling(ScalarFunction &func, FunctionNullHandling handling) {
	func.null_handling = handling;
}

#endif

// --- Direct flat-vector writes (cross-version) ---
// DuckDB main's FlatVector::GetData<T>(Vector&) and FlatVector::Validity(Vector&)
// return const-qualified by default; new GetDataMutable<T> / ValidityMutable
// overloads exist for writes. On v1.4.x/v1.5.x the returns are already non-const,
// so the const_cast is a no-op there.
//
// See duckdb_markdown's docs/DUCKDB_API_MIGRATION.md §4 for the long-form rationale.
template <typename T>
inline T *CompatGetDataMutable(Vector &vec) {
	return const_cast<T *>(FlatVector::GetData<T>(vec));
}

inline ValidityMask &CompatGetValidityMutable(Vector &vec) {
	return const_cast<ValidityMask &>(FlatVector::Validity(vec));
}

} // namespace duckdb
