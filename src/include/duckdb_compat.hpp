#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/prepared_statement.hpp"
#include "duckdb/planner/expression/bound_parameter_data.hpp"
#include <type_traits>
#include <utility>

// duckdb_compat.hpp — fleet-standard cross-version shim for DuckDB extensions.
//
// Cross-version coverage:
//   - duckdb v1.5.x (this extension's pin): old API everywhere
//   - duckdb main / v2.0-cyanoptera:        new API everywhere
//
// DETECT FEATURES, NOT VERSIONS -- AND NOT PROXY HEADERS EITHER.
//
// An earlier revision of this header followed the shape established by
// duckdb_webbed#76: `__has_include("duckdb/common/vector/list_vector.hpp")`,
// then one `#ifdef` gating every shim at once. That is two mistakes compounded.
//
//  1. A header's PRESENCE is not the change you care about. DuckDB backports
//     headers to the stable branch without the signature changes that shipped
//     alongside them on main -- identifier.hpp did exactly that on
//     v1.5-variegata -- so the probe flips to "new API" on a DuckDB that still
//     has the old one, and every shim in the file picks the wrong branch at once.
//  2. One macro for many changes assumes they always land together. They do not
//     have to, and when they come apart the single #ifdef is guaranteed to be
//     wrong about all but one of them.
//
// So: `#if __has_include(...)` appears below ONLY to decide whether a header can
// be included, never to decide what an API looks like. Every behavioural shim is
// selected by probing for the member or the type that actually changed, and each
// change is probed SEPARATELY.
//
// The probes are written as tag dispatch rather than `if constexpr` so this
// header also compiles at C++11: several extensions in this fleet build their
// TUs at C++11 on purpose (forcing C++17 on the extension but not on libduckdb
// gives static-const members in duckdb's headers implicit inline linkage in one
// set of TUs and not the other, which produces multiple-definition link errors).
// Tag dispatch has the property that matters here -- only the selected overload
// is instantiated, so the branch naming an absent member is never compiled.
//
// See teaguesterling/duckdb_markdown's docs/duckdb_v2_migration.md for the
// long-form rationale + upgrade checklist for other extensions.

// v2.0 split the per-vector accessor classes out of
// duckdb/common/types/vector.hpp into one header each under
// duckdb/common/vector/, and duckdb.hpp no longer pulls them in transitively.
// Included (not probed-on) so that TUs naming ListVector/StructVector at
// namespace scope keep compiling; the absence of these headers is not used to
// infer anything about any API.
#if __has_include("duckdb/common/vector/list_vector.hpp")
#include "duckdb/common/vector/list_vector.hpp"
#endif
#if __has_include("duckdb/common/vector/struct_vector.hpp")
#include "duckdb/common/vector/struct_vector.hpp"
#endif

namespace duckdb {

// --- Output chunk finalization (change class 18) ------------------------------
// v2.0 gives every Vector its own size, and DataChunk::SetCardinality no longer
// sets it -- it updates only the chunk's count. Vector::SetValue writes AT AN
// INDEX and never advances a vector's size, so a table function that fills its
// output with SetValue and then calls SetCardinality leaves every child vector
// at size 0.
//
// The failure is SILENT, which is what makes it worth a shim rather than a fix
// at the call site. Readers that go through the chunk count -- the printer,
// scalar functions -- are correct, so casual testing passes. `IS NULL` iterates
// the VECTOR's size, finds zero rows, writes nothing, and the result buffer
// keeps its default `false`. So `SELECT c` prints NULL while `SELECT c IS NULL`
// returns false, in the same result set, with no exception and a green build.
//
// SetChildCardinality is the call that sizes the children, and for an
// index-writing caller it is REQUIRED, not merely safe. (It would be a no-op for
// a caller that filled the chunk with Vector::Append, which advances v_size as
// it goes -- this extension has no such caller: `grep -rn '\.Append(\|AppendValue' src/`
// is empty, and every table-function scan here writes with DataChunk::SetValue.)
//
// Probed on the member itself. The header probe this replaced keyed off
// list_vector.hpp, which shipped in the same upstream PR as per-vector size
// tracking -- true today, and an assumption with no reason to keep holding.
template <class T, class = void>
struct CompatHasSetChildCardinality : std::false_type {};
template <class T>
struct CompatHasSetChildCardinality<T, decltype(void(std::declval<T &>().SetChildCardinality(idx_t(0))))>
    : std::true_type {};

// The Impl overloads MUST be templates. Tag dispatch only defers compilation of
// the unselected branch when that branch is a template -- a non-template
// overload naming an absent member is a hard error at declaration time,
// whichever tag is passed. (Observed: "class duckdb::DataChunk has no member
// named SetChildCardinality" on the pinned build, from the branch that is never
// called there.)
template <class CHUNK>
inline void CompatSetOutputCardinalityImpl(CHUNK &chunk, idx_t count, std::true_type) {
	chunk.SetChildCardinality(count);
}
template <class CHUNK>
inline void CompatSetOutputCardinalityImpl(CHUNK &chunk, idx_t count, std::false_type) {
	chunk.SetCardinality(count);
}
//! Finalise a table function's output chunk. Use this rather than
//! DataChunk::SetCardinality anywhere the chunk was filled with SetValue.
inline void CompatSetOutputCardinality(DataChunk &chunk, idx_t count) {
	CompatSetOutputCardinalityImpl(chunk, count, CompatHasSetChildCardinality<DataChunk>());
}

// --- ScalarFunction property setters ------------------------------------------
// v2.0 made BaseScalarFunction's property fields private behind accessors. The
// accessors (SetNullHandling, SetStability, SetFallible, ...) are ALSO present
// on the pinned v1.5 -- verified at function.hpp:192-217 of the pin -- so there
// is nothing to branch on: call the accessor on both lines. Reaching for
// `func.null_handling = ...` on v1.5 only creates a v2.0 compile error for no
// benefit.
inline void SetScalarFunctionNullHandling(ScalarFunction &func, FunctionNullHandling handling) {
	func.SetNullHandling(handling);
}

// --- Constant folding workaround (from duckdb_webbed PR #76) ------------------
// DuckDB main's VectorStructBuffer::SetVectorType throws InternalException when
// the optimizer constant-folds a function returning a STRUCT-containing type
// (LIST(STRUCT), STRUCT, MAP), because of the child-size mismatch described
// above. Marking such a function VOLATILE skips constant folding.
//
// Deliberately still conditional, and deliberately NOT applied on the pinned
// line: VOLATILE is optimizer-visible, and this shim exists to avoid a v2.0
// crash rather than to restate a property. (An argument exists that mcp_server_*
// should be VOLATILE on both lines on its own merits -- they start and stop a
// server -- but that is a behaviour change to the shipped binary and belongs in
// its own commit with its own measurement, not smuggled in under a compat shim.)
//
// Gated on the per-vector-size-tracking probe above rather than on a header,
// because that IS the upstream change that produces the child-size mismatch.
inline void PreventStructConstantFoldingImpl(ScalarFunction &func, std::true_type) {
	func.SetStability(FunctionStability::VOLATILE);
}
inline void PreventStructConstantFoldingImpl(ScalarFunction &, std::false_type) {
}
inline void PreventStructConstantFolding(ScalarFunction &func) {
	PreventStructConstantFoldingImpl(func, CompatHasSetChildCardinality<DataChunk>());
}

//===--------------------------------------------------------------------===//
// DuckDB v2.0 (main / v2.0-cyanoptera) shims
//===--------------------------------------------------------------------===//

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
// a v1.5 that still wants strings, and every bind signature stops compiling.
//
// AND BE PRECISE ABOUT *WHICH* DECLARATION YOU DERIVE FROM. "Some container of
// names in the same header" is the right kind of answer aimed at the wrong
// thing. Three separate upstream declarations all flipped string -> Identifier
// in v2.0 and they agree TODAY, but they are independent and can be changed,
// backported or reverted one at a time:
//
//   table_function_bind_t's 4th parameter   <- the bind-name boundary (THIS one)
//   TableFunctionBindInput::input_table_names <- names of INPUT TABLES to a
//                                                table-in/table-out function
//   child_list_t<T>'s key                   <- STRUCT FIELD names
//
// An earlier revision of this header read the type off `input_table_names`,
// which is a sibling that happens to move in step. The thing this alias exists
// to describe is the out-parameter every bind callback in this extension has to
// declare, so derive it from the typedef of that callback itself. Then there is
// no "happens to" left: the type named IS the type that changed, and every bind
// signature follows automatically.
template <class T>
struct CompatBindNamesOf;
template <class R, class A, class B, class C, class D>
struct CompatBindNamesOf<R (*)(A, B, C, D)> {
	using type = typename std::remove_reference<D>::type::value_type;
};
//! The type DuckDB uses for column names in bind signatures: `string` on v1.5,
//! `Identifier` on v2.0.
using CompatName = CompatBindNamesOf<table_function_bind_t>::type;

//! Read a name back out as a plain string. Both overloads exist wherever both
//! types do; on v2.0 Identifier -> string is explicit, so this is the opt-in.
inline string CompatNameStr(const string &name) {
	return name;
}
#ifdef DUCKDB_HAS_IDENTIFIER
// Declares the Identifier overload only; it does NOT decide what CompatName is.
// Both overloads coexist happily on a DuckDB that has Identifier but still binds
// with strings -- which is exactly the v1.5 branch tip.
inline string CompatNameStr(const Identifier &name) {
	return name.GetIdentifierName();
}
#endif

// PIN THE DERIVATION. Deriving CompatName fixes the type but leaves a second
// failure mode open: CompatName could resolve to Identifier on a DuckDB whose
// identifier.hpp this header did not find, so the overload above was never
// declared -- and then CompatNameStr either fails to match or silently picks a
// worse conversion. Assert the coupling rather than assuming it.
static_assert(std::is_same<decltype(CompatNameStr(std::declval<const CompatName &>())), string>::value,
              "CompatNameStr must accept the derived CompatName on every DuckDB line");

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

// --- QueryResult names and types ----------------------------------------------
// v1.5: BaseQueryResult::names (vector<string>) and ::types are PUBLIC fields
// v2.0: both are private; GetNames() -> const vector<Identifier> &
//                        GetTypes() -> const vector<LogicalType> &
//
// The accessors do not exist on v1.5, so this is a positive v2.0 probe.
template <class T, class = void>
struct CompatHasResultGetNames : std::false_type {};
template <class T>
struct CompatHasResultGetNames<T, decltype(void(std::declval<const T &>().GetNames()))> : std::true_type {};

// DELIBERATELY NOT `vector<CompatName>`. Result column names and table-function
// bind names are two DIFFERENT upstream declarations (change class 17 vs class
// 2) that both flipped string -> Identifier in v2.0. They agree today, which is
// exactly what makes writing `vector<CompatName>` here feel harmless -- and it
// is the same mistake as probing identifier.hpp: it silently binds this shim's
// correctness to a change it does not describe. Read the container type off the
// result itself so the two can diverge without breaking either.
//
// A class-template specialisation rather than an overload pair: only the
// selected specialisation is instantiated, so the v1.5 branch naming the (on
// v2.0 private) `names` member is never compiled there, and there is no
// overload-resolution substitution to reason about.
template <class RESULT, bool HAS_ACCESSOR>
struct CompatResultNamesTypeImpl;
template <class RESULT>
struct CompatResultNamesTypeImpl<RESULT, true> {
	using type = typename std::remove_const<
	    typename std::remove_reference<decltype(std::declval<const RESULT &>().GetNames())>::type>::type;
};
template <class RESULT>
struct CompatResultNamesTypeImpl<RESULT, false> {
	using type = typename std::remove_const<
	    typename std::remove_reference<decltype(std::declval<const RESULT &>().names)>::type>::type;
};
//! The container a query result stores its column names in: vector<string> on
//! v1.5, vector<Identifier> on v2.0.
template <class RESULT>
struct CompatResultNamesType : CompatResultNamesTypeImpl<RESULT, CompatHasResultGetNames<RESULT>::value> {};

template <class RESULT>
inline const typename CompatResultNamesType<RESULT>::type &CompatResultNamesImpl(const RESULT &result, std::true_type) {
	return result.GetNames();
}
template <class RESULT>
inline const typename CompatResultNamesType<RESULT>::type &CompatResultNamesImpl(const RESULT &result,
                                                                                 std::false_type) {
	return result.names;
}
//! The column names of a query result. Run an element through CompatNameStr to
//! use it as a string.
template <class RESULT>
inline const typename CompatResultNamesType<RESULT>::type &CompatResultNames(const RESULT &result) {
	return CompatResultNamesImpl(result, CompatHasResultGetNames<RESULT>());
}

template <class T, class = void>
struct CompatHasResultGetTypes : std::false_type {};
template <class T>
struct CompatHasResultGetTypes<T, decltype(void(std::declval<const T &>().GetTypes()))> : std::true_type {};

template <class RESULT>
inline const vector<LogicalType> &CompatResultTypesImpl(const RESULT &result, std::true_type) {
	return result.GetTypes();
}
template <class RESULT>
inline const vector<LogicalType> &CompatResultTypesImpl(const RESULT &result, std::false_type) {
	return result.types;
}
//! The column types of a query result.
template <class RESULT>
inline const vector<LogicalType> &CompatResultTypes(const RESULT &result) {
	return CompatResultTypesImpl(result, CompatHasResultGetTypes<RESULT>());
}

// --- STRUCT field names -------------------------------------------------------
// child_list_t<T> is vector<pair<string, T>> on v1.5 and vector<pair<Identifier, T>>
// on v2.0, and StructType::GetChildName returns the matching type.
//
// This one is worth reading twice, because the compiler will NOT catch the part
// that matters. `field_name == "name"` keeps compiling on v2.0 -- Identifier has
// operator== against const char* and string -- but Identifier's equality is
// CASE-INSENSITIVE, so a comparison that was exact on v1.5 silently starts
// matching "Name" and "NAME" on v2.0. These are JSON-RPC object keys off the
// wire, where case-sensitive is the protocol's rule, so every comparison goes
// through the raw string and keeps the v1.5 behaviour on both versions.
inline string CompatStructFieldName(const LogicalType &type, idx_t index) {
	return CompatNameStr(StructType::GetChildName(type, index));
}

// --- Value type reinterpretation ----------------------------------------------
// v1.5: void Value::Reinterpret(LogicalType)   -- mutates in place
// v2.0: Value Value::WithType(LogicalType) const -- returns a copy
// Neither converts the underlying value; both just relabel its type. (The cast
// is DefaultCastAs, which is a different thing and is unchanged.)
template <class T, class = void>
struct CompatHasValueWithType : std::false_type {};
template <class T>
struct CompatHasValueWithType<T, decltype(void(std::declval<const T &>().WithType(std::declval<LogicalType>())))>
    : std::true_type {};

template <class VALUE>
inline VALUE CompatWithTypeImpl(VALUE value, LogicalType type, std::true_type) {
	return value.WithType(std::move(type));
}
template <class VALUE>
inline VALUE CompatWithTypeImpl(VALUE value, LogicalType type, std::false_type) {
	value.Reinterpret(std::move(type));
	return value;
}
//! Concrete entry point on purpose -- see the note on CompatWithAlias about why a
//! defaulted template parameter is the wrong shape here.
inline Value CompatWithType(Value value, LogicalType type) {
	return CompatWithTypeImpl(std::move(value), std::move(type), CompatHasValueWithType<Value>());
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
using CompatNamedParamMap = typename std::conditional<CompatHasNamedParamMapAccessor<PreparedStatement>::value,
                                                      identifier_map_t<VALUE>, case_insensitive_map_t<VALUE>>::type;
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
inline ValidityMask &CompatFlatValidityMutable(Vector &vec) {
	return CompatFlatValidityMutableImpl<FV>(vec, CompatHasFlatValidityMutable<FV>());
}

//! Long-standing spelling in this repo; the fleet-standard name is
//! CompatFlatValidityMutable. Kept as a forwarder so the call sites do not churn.
template <class FV = FlatVector>
inline ValidityMask &CompatGetValidityMutable(Vector &vec) {
	return CompatFlatValidityMutable<FV>(vec);
}

} // namespace duckdb
