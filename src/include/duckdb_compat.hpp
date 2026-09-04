#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/prepared_statement.hpp"
#include "duckdb/planner/expression/bound_parameter_data.hpp"
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

// --- Prepared statement named parameters --------------------------------------
// v2.0 re-keyed the named-parameter maps on Identifier and moved the map behind
// an accessor:
//
//   v1.5:  PreparedStatement::named_param_map      case_insensitive_map_t<idx_t>   (public field)
//          Execute(case_insensitive_map_t<BoundParameterData> &)
//   v2.0:  PreparedStatement::GetNamedParameterMap()  identifier_map_t<idx_t>
//          Execute(identifier_map_t<BoundParameterData> &)
//
// Probed on the v2.0-only accessor. Deliberately NOT probed by trying to call
// Execute with an identifier_map_t: PreparedStatement has a variadic
// `template <class... ARGS> Execute(ARGS...)` overload that swallows any
// argument, so such a probe answers "yes" on both versions and then fails deep
// inside the variadic instantiation.
template <class T, class = void>
struct CompatHasNamedParamMapAccessor : std::false_type {};
template <class T>
struct CompatHasNamedParamMapAccessor<T, decltype(void(std::declval<const T &>().GetNamedParameterMap()))>
    : std::true_type {};

//! The map type PreparedStatement::Execute accepts for named parameters.
#ifdef DUCKDB_HAS_IDENTIFIER
template <class VALUE>
using CompatNamedParamMap =
    typename std::conditional<CompatHasNamedParamMapAccessor<PreparedStatement>::value, identifier_map_t<VALUE>,
                              case_insensitive_map_t<VALUE>>::type;
#else
template <class VALUE>
using CompatNamedParamMap = case_insensitive_map_t<VALUE>;
#endif

template <class STMT, class NAME>
inline bool CompatHasNamedParamImpl(const STMT &stmt, const NAME &name, std::true_type) {
	return stmt.GetNamedParameterMap().count(name) > 0;
}
template <class STMT, class NAME>
inline bool CompatHasNamedParamImpl(const STMT &stmt, const NAME &name, std::false_type) {
	return stmt.named_param_map.count(name) > 0;
}
//! Does this prepared statement declare a parameter with this name?
template <class STMT, class NAME>
inline bool CompatHasNamedParam(const STMT &stmt, const NAME &name) {
	return CompatHasNamedParamImpl(stmt, name, CompatHasNamedParamMapAccessor<STMT>());
}

// --- Fallible scalar functions ------------------------------------------------
// v2.0 requires a scalar function that can throw at EXECUTION time to say so.
// Throwing from one that has not becomes:
//
//   INTERNAL Error: Scalar function "f" threw an execution error, but the
//   function is not marked as fallible - the function must call SetFallible().
//
// This is not a compile error and nothing in the API is greppable for it. The
// only symptom is a test that exercises an error path, and enforcement is an
// assertion -- so one CI arch can be green while another is red on the very same
// commit, purely because only one image builds with assertions on.
//
// BaseScalarFunction::SetFallible() also exists on the pinned v1.5 (it just is
// not enforced there), so this is a shim only for the sake of an older pin.
// Must be called BEFORE the function goes into a FunctionSet, since v2.0's set
// members are no longer mutable.
template <class T, class = void>
struct CompatHasSetFallible : std::false_type {};
template <class T>
struct CompatHasSetFallible<T, decltype(void(std::declval<T &>().SetFallible()))> : std::true_type {};

template <class FUNC>
inline void CompatSetFallibleImpl(FUNC &fun, std::true_type) {
	fun.SetFallible();
}
template <class FUNC>
inline void CompatSetFallibleImpl(FUNC &, std::false_type) {
}
template <class FUNC>
inline void CompatSetFallible(FUNC &fun) {
	CompatSetFallibleImpl(fun, CompatHasSetFallible<FUNC>());
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
// The entry point is CONCRETE, not `template <class TYPE = LogicalType>`: a
// default template argument is inert because deduction wins, so
//
//     CompatWithAlias(LogicalType::VARCHAR, "md")
//
// would deduce TYPE = LogicalTypeId -- LogicalType::VARCHAR is a static
// constexpr LogicalTypeId, not a LogicalType -- and hard-error inside the shim
// on the PINNED build. A concrete parameter restores the implicit
// LogicalTypeId -> LogicalType conversion at the call site.
inline LogicalType CompatWithAlias(LogicalType type, string alias) {
	return CompatWithAliasImpl(std::move(type), std::move(alias), CompatHasWithAlias<LogicalType>());
}

// --- Vector::ToUnifiedFormat ---------------------------------------------------
// v1.5: ToUnifiedFormat(count, data)  -- the only overload
// v2.0: ToUnifiedFormat(data)         -- plus the count form kept as [[deprecated]]
//
// PROBE FOR THE COUNT-FREE OVERLOAD, not the count-taking one. v2.0 did not
// remove the count form, it deprecated it, so a probe for the count form is true
// on BOTH versions and the shim would always take the deprecated path -- silently
// never reaching the API it exists to call. Probe for what exists ONLY on v2.0.
// Unused in this extension today; carried for fleet interchangeability.
template <class T, class = void>
struct CompatToUnifiedWithoutCount : std::false_type {};
template <class T>
struct CompatToUnifiedWithoutCount<T, decltype(void(std::declval<T &>().ToUnifiedFormat(
                                          std::declval<UnifiedVectorFormat &>())))> : std::true_type {};

template <class VEC>
inline void CompatToUnifiedFormatImpl(VEC &vec, idx_t, UnifiedVectorFormat &data, std::true_type) {
	vec.ToUnifiedFormat(data);
}
template <class VEC>
inline void CompatToUnifiedFormatImpl(VEC &vec, idx_t count, UnifiedVectorFormat &data, std::false_type) {
	vec.ToUnifiedFormat(count, data);
}
template <class VEC = Vector>
inline void CompatToUnifiedFormat(VEC &vec, idx_t count, UnifiedVectorFormat &data) {
	CompatToUnifiedFormatImpl(vec, count, data, CompatToUnifiedWithoutCount<VEC>());
}

// --- Direct flat-vector writes ------------------------------------------------
// v1.5: FlatVector::GetData<T>(vec)         returns T*
//       FlatVector::Validity(vec)           returns ValidityMask&
// v2.0: FlatVector::GetData<T>(vec)         returns const T*
//       FlatVector::GetDataMutable<T>(vec)  returns T*
//       FlatVector::Validity(vec)           returns const ValidityMask&
//       FlatVector::ValidityMutable(vec)    returns ValidityMask&
//
// A const_cast off the v2.0 READ accessor is NOT an equivalent -- it compiles and
// then silently does the wrong thing. On v2.0 the two accessors reach the buffer
// differently: GetData/Validity go through Vector::GetBufferRef()/Buffer(), while
// GetDataMutable/ValidityMutable go through Vector::BufferMutable(), which
// un-shares a copy-on-write buffer first. Writing through the const_cast can
// therefore scribble into a buffer another vector still shares. These shims call
// the real mutable accessor wherever it exists.
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

//! Long-standing spelling in this repo (~30 call sites); the fleet-standard name
//! is CompatFlatDataMutable. Kept as a forwarder so the call sites do not churn.
template <class T, class FV = FlatVector>
inline T *CompatGetDataMutable(Vector &vec) {
	return CompatFlatDataMutable<T, FV>(vec);
}

template <class T, class = void>
struct CompatHasFlatValidityMutable : std::false_type {};
template <class T>
struct CompatHasFlatValidityMutable<T, decltype(void(T::ValidityMutable(std::declval<Vector &>())))> : std::true_type {
};

template <class FV>
inline ValidityMask &CompatFlatValidityMutableImpl(Vector &vec, std::true_type) {
	return FV::ValidityMutable(vec);
}
template <class FV>
inline ValidityMask &CompatFlatValidityMutableImpl(Vector &vec, std::false_type) {
	// Non-const already on v1.5, so this cast is a no-op there.
	return const_cast<ValidityMask &>(FV::Validity(vec));
}
template <class FV = FlatVector>
inline ValidityMask &CompatGetValidityMutable(Vector &vec) {
	return CompatFlatValidityMutableImpl<FV>(vec, CompatHasFlatValidityMutable<FV>());
}

} // namespace duckdb
