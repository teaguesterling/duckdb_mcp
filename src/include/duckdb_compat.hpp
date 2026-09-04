#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include <type_traits>
#include <utility>

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

// --- Constant folding workaround (from duckdb_webbed PR #76) ---
// DuckDB main's VectorStructBuffer::SetVectorType throws InternalException when
// the optimizer constant-folds functions returning STRUCT-containing types
// (LIST(STRUCT), STRUCT, MAP) due to child size mismatches. Marking them
// VOLATILE skips constant folding. Semantically correct for mcp_server_*
// functions anyway — they have side effects (start/stop server, status).
inline void PreventStructConstantFolding(ScalarFunction &func) {
	func.SetStability(FunctionStability::VOLATILE);
}

#else // Old API (v1.4.x / v1.5.x)

inline void CompatSetOutputCardinality(DataChunk &chunk, idx_t count) {
	chunk.SetCardinality(count);
}

inline void SetScalarFunctionNullHandling(ScalarFunction &func, FunctionNullHandling handling) {
	func.null_handling = handling;
}

// No-op on old API — constant folding works fine for complex types
inline void PreventStructConstantFolding(ScalarFunction &) {
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

//===--------------------------------------------------------------------===//
// DuckDB v2.0 (main / v2.0-cyanoptera) shims
//===--------------------------------------------------------------------===//
//
// Everything below is detected by PROBING FOR THE THING ITSELF rather than by a
// version macro or a proxy header, and each change is probed SEPARATELY. Tying
// several changes to one macro silently picks the wrong branch the moment they
// land in different releases -- which, as the CompatName note below records, has
// already happened once on the v1.5 line.
//
// The probes are written as tag dispatch rather than `if constexpr` so that this
// header also compiles at C++11: several extensions in this fleet build their
// TUs at C++11 on purpose (forcing C++17 on the extension but not on libduckdb
// gives static-const members in duckdb's headers implicit inline linkage in one
// set of TUs and not the other, which produces multiple-definition link errors).
// Tag dispatch has the property that matters here -- only the selected overload
// is instantiated, so the branch naming an absent member is never compiled.

// --- Detection: does this DuckDB have duckdb::Identifier? --------------------
// Needed only so that CompatNameStr can accept one. It is NOT used to decide
// what CompatName is -- see the note on that below.
#if __has_include("duckdb/common/identifier.hpp")
#define DUCKDB_HAS_IDENTIFIER 1
#include "duckdb/common/identifier.hpp"
#endif

// --- bind-signature name type ------------------------------------------------
// v2.0 replaced std::string with duckdb::Identifier as the name type in
// table-function and COPY bind signatures. Identifier compares case-insensitively,
// and on v2.0 construction from a RUNTIME string is explicit by design, so a
// boundary helper is needed rather than an implicit conversion.
//
// NOTE ON THE PROBE. The obvious probe -- __has_include of identifier.hpp -- is
// WRONG, and the fleet's other headers currently get this wrong. The v1.5 line
// backported duckdb/common/identifier.hpp (with implicit string<->Identifier
// conversions) WITHOUT changing table_function_bind_t, which still takes
// `vector<string> &names` there:
//
//   v1.5-variegata @ b155d6f6 (our pin): no identifier.hpp,   bind: vector<string>
//   v1.5-variegata @ branch tip:         HAS identifier.hpp,  bind: vector<string>
//   main (v2.0):                         HAS identifier.hpp,  bind: vector<Identifier>
//
// So on the next submodule bump a header probe starts reporting "v2.0 names" on
// a v1.5 that still wants strings, and every bind signature stops compiling. The
// probe below cannot drift: CompatName is *defined as* whatever element type this
// DuckDB's own bind input uses for column names.
//! The type DuckDB uses for column names in bind signatures: `string` on v1.5,
//! `Identifier` on v2.0. Read off the container's own value_type so it does not
//! depend on duckdb::vector's template parameter list either.
using CompatName = std::remove_reference<decltype(std::declval<TableFunctionBindInput &>()
                                                      .input_table_names)>::type::value_type;

//! Read a name back out as a plain string. Both overloads exist wherever both
//! types do; on v2.0 Identifier -> string is explicit, so this is the opt-in.
inline string CompatNameStr(const string &name) {
	return name;
}
#ifdef DUCKDB_HAS_IDENTIFIER
inline string CompatNameStr(const Identifier &name) {
	return name.GetIdentifierName();
}
#endif

//! Promote a RUNTIME string to a bind name. Literals need no helper --
//! `names.emplace_back("file_path")` compiles unchanged on both versions,
//! because Identifier's constructor from a string literal stays implicit.
inline CompatName CompatMakeName(string name) {
	return CompatName(std::move(name));
}

// --- Catalog parsed-data: name/schema fields became accessors -----------------
// v2.0 folded catalog/schema/name into a single private QualifiedName on
// CreateInfo and DropInfo, so `info.schema` and `info.name` no longer exist.
// This is not covered by the migration guide; it only bites extensions that
// implement their own Catalog/SchemaCatalogEntry (i.e. a storage extension).
//
//   v1.5 @ our pin:  CreateInfo::schema, DropInfo::name           (public fields)
//   v2.0:            GetQualifiedName().Schema() / .Name()        (accessors)
//
// Probed on GetQualifiedName rather than on the absent field, so the new branch
// is the one selected whenever it is available. The v1.5 branch tip backported
// GetQualifiedName too, where it returns by VALUE -- hence every helper here
// copies out of it inside a single full-expression and never binds a reference.
template <class T, class = void>
struct CompatHasQualifiedName : std::false_type {};
template <class T>
struct CompatHasQualifiedName<T, decltype(void(std::declval<const T &>().GetQualifiedName()))> : std::true_type {};

template <class INFO>
inline string CompatEntryNameImpl(const INFO &info, std::true_type) {
	return CompatNameStr(info.GetQualifiedName().Name());
}
template <class INFO>
inline string CompatEntryNameImpl(const INFO &info, std::false_type) {
	return info.name;
}
//! The name of the entry a DropInfo refers to.
template <class INFO>
inline string CompatEntryName(const INFO &info) {
	return CompatEntryNameImpl(info, CompatHasQualifiedName<INFO>());
}

template <class INFO>
inline string CompatSchemaNameImpl(const INFO &info, std::true_type) {
	return CompatNameStr(info.GetQualifiedName().Schema());
}
template <class INFO>
inline string CompatSchemaNameImpl(const INFO &info, std::false_type) {
	return info.schema;
}
//! The schema a CreateInfo refers to. For CreateSchemaInfo this is the schema
//! BEING CREATED: v2.0 stores it in the Schema() slot of the qualified name
//! (that is exactly what CreateSchemaInfo::SchemaName() reads), and v1.5 stores
//! it in CreateInfo::schema.
template <class INFO>
inline string CompatSchemaName(const INFO &info) {
	return CompatSchemaNameImpl(info, CompatHasQualifiedName<INFO>());
}

// Probed separately from the getter: the setter and the getter are different
// members and there is no guarantee they arrive together.
template <class T, class = void>
struct CompatHasSetSchema : std::false_type {};
template <class T>
struct CompatHasSetSchema<T, decltype(void(std::declval<T &>().SetSchema(std::declval<CompatName>())))>
    : std::true_type {};

template <class INFO>
inline void CompatSetSchemaNameImpl(INFO &info, string name, std::true_type) {
	info.SetSchema(CompatMakeName(std::move(name)));
}
template <class INFO>
inline void CompatSetSchemaNameImpl(INFO &info, string name, std::false_type) {
	info.schema = std::move(name);
}
//! Set the schema a CreateInfo refers to; see CompatSchemaName for what that
//! means on a CreateSchemaInfo.
template <class INFO>
inline void CompatSetSchemaName(INFO &info, string name) {
	CompatSetSchemaNameImpl(info, std::move(name), CompatHasSetSchema<INFO>());
}

// --- LogicalType alias --------------------------------------------------------
// v1.5: void SetAlias(string)                -- mutates in place
// v2.0: LogicalType WithAlias(string) const  -- returns a copy, never mutating a
//       type whose type-info may be shared. SetAlias is REMOVED, not deprecated.
//
// Unused in this extension today; carried so the fleet's compat headers stay
// interchangeable. (Note for anyone porting a call site: SetAlias was a setter,
// so calling it twice REPLACED the alias rather than adding one -- only the last
// call ever took effect.)
template <class T, class = void>
struct CompatHasWithAlias : std::false_type {};
template <class T>
struct CompatHasWithAlias<T, decltype(void(std::declval<const T &>().WithAlias(string())))> : std::true_type {};

template <class TYPE>
inline LogicalType CompatWithAliasImpl(TYPE type, string alias, std::true_type) {
	return type.WithAlias(std::move(alias));
}
template <class TYPE>
inline LogicalType CompatWithAliasImpl(TYPE type, string alias, std::false_type) {
	type.SetAlias(std::move(alias));
	return type;
}
template <class TYPE = LogicalType>
inline LogicalType CompatWithAlias(TYPE type, string alias) {
	return CompatWithAliasImpl(std::move(type), std::move(alias), CompatHasWithAlias<TYPE>());
}

// --- Vector::ToUnifiedFormat ---------------------------------------------------
// v2.0 dropped the count parameter (the old form is deprecated, not removed).
// Unused in this extension today; carried for fleet interchangeability.
template <class T, class = void>
struct CompatToUnifiedTakesCount : std::false_type {};
template <class T>
struct CompatToUnifiedTakesCount<T, decltype(void(std::declval<T &>().ToUnifiedFormat(
                                        idx_t(0), std::declval<UnifiedVectorFormat &>())))> : std::true_type {};

template <class VEC>
inline void CompatToUnifiedFormatImpl(VEC &vec, idx_t count, UnifiedVectorFormat &data, std::true_type) {
	vec.ToUnifiedFormat(count, data);
}
template <class VEC>
inline void CompatToUnifiedFormatImpl(VEC &vec, idx_t, UnifiedVectorFormat &data, std::false_type) {
	vec.ToUnifiedFormat(data);
}
template <class VEC = Vector>
inline void CompatToUnifiedFormat(VEC &vec, idx_t count, UnifiedVectorFormat &data) {
	CompatToUnifiedFormatImpl(vec, count, data, CompatToUnifiedTakesCount<VEC>());
}

// --- FlatVector mutable data ---------------------------------------------------
// v1.5: FlatVector::GetData<T>(vec)         returns T*
// v2.0: FlatVector::GetData<T>(vec)         returns const T*
//       FlatVector::GetDataMutable<T>(vec)  returns T*
//
// CompatGetDataMutable above already covers this repo's ~30 write sites with a
// const_cast, which is well-defined here (the buffer is not really const) and is
// left in place rather than churned. This is the fleet-standard spelling that
// asks for mutability through the real v2.0 accessor.
template <class T, class = void>
struct CompatHasFlatGetDataMutable : std::false_type {};
template <class T>
struct CompatHasFlatGetDataMutable<T, decltype(void(T::template GetDataMutable<bool>(std::declval<Vector &>())))>
    : std::true_type {};

template <class VALUE, class FV>
inline VALUE *CompatFlatDataMutableImpl(Vector &vec, std::true_type) {
	return FV::template GetDataMutable<VALUE>(vec);
}
template <class VALUE, class FV>
inline VALUE *CompatFlatDataMutableImpl(Vector &vec, std::false_type) {
	return FV::template GetData<VALUE>(vec);
}
template <class VALUE, class FV = FlatVector>
inline VALUE *CompatFlatDataMutable(Vector &vec) {
	return CompatFlatDataMutableImpl<VALUE, FV>(vec, CompatHasFlatGetDataMutable<FV>());
}

} // namespace duckdb
