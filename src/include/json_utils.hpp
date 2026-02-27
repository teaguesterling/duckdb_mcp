//===----------------------------------------------------------------------===//
//                         DuckDB
//
// json_utils.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "yyjson.hpp"
#include <string>
#include <memory>

using namespace duckdb_yyjson;

namespace duckdb {

//! JSON utility class for MCP message serialization/deserialization
class JSONUtils {
public:
	//! Create a JSON object
	static yyjson_mut_doc *CreateDocument();

	//! Create a JSON object within a document
	static yyjson_mut_val *CreateObject(yyjson_mut_doc *doc);

	//! Create a JSON array within a document
	static yyjson_mut_val *CreateArray(yyjson_mut_doc *doc);

	//! Add a string value to an object
	static void AddString(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const string &value);

	//! Add an integer value to an object
	static void AddInt(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, int64_t value);

	//! Add a boolean value to an object
	static void AddBool(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, bool value);

	//! Add a null value to an object
	static void AddNull(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key);

	//! Add an object to an object
	static void AddObject(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key, yyjson_mut_val *child);

	//! Add an array to an object
	static void AddArray(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key, yyjson_mut_val *child);

	//! Add a value to an array
	static void ArrayAdd(yyjson_mut_doc *doc, yyjson_mut_val *arr, yyjson_mut_val *val);

	//! Add a string to an array
	static void ArrayAddString(yyjson_mut_doc *doc, yyjson_mut_val *arr, const string &value);

	//! Serialize document to string
	static string Serialize(yyjson_mut_doc *doc);

	//! Parse JSON string to document
	static yyjson_doc *Parse(const string &json);

	//! Free a mutable document
	static void FreeDocument(yyjson_mut_doc *doc);

	//! Free a read-only document
	static void FreeDocument(yyjson_doc *doc);

	//! Convert DuckDB Value to JSON value
	static yyjson_mut_val *ValueToJSON(yyjson_mut_doc *doc, const Value &value);

	//! Convert query results to JSON array
	static yyjson_mut_val *QueryResultToJSON(yyjson_mut_doc *doc, const unique_ptr<QueryResult> &result);

	//! Create MCP message object with common fields
	static yyjson_mut_val *CreateMCPMessage(yyjson_mut_doc *doc, const char *jsonrpc = "2.0");

	//! Get string value from JSON object (only for string types)
	static string GetString(yyjson_val *obj, const char *key, const string &default_value = "");

	//! Get any value as string representation (works for int, bool, string, etc.)
	static string GetValueAsString(yyjson_val *obj, const char *key, const string &default_value = "");

	//! Get int value from JSON object
	static int64_t GetInt(yyjson_val *obj, const char *key, int64_t default_value = 0);

	//! Get bool value from JSON object
	static bool GetBool(yyjson_val *obj, const char *key, bool default_value = false);

	//! Get object value from JSON object
	static yyjson_val *GetObject(yyjson_val *obj, const char *key);

	//! Get array value from JSON object
	static yyjson_val *GetArray(yyjson_val *obj, const char *key);
};

//! Helper class for parsing JSON tool arguments
//! Supports both JSON string input (from MCP protocol) and STRUCT Value input (from SQL calls)
class JSONArgumentParser {
public:
	//! Parse arguments - accepts either VARCHAR (JSON string) or STRUCT Value
	//! Returns true if parsing succeeded, stores parsed doc internally
	bool Parse(const Value &arguments);

	//! Check if a field exists
	bool HasField(const string &name) const;

	//! Get string field (empty string if not found, only for string types)
	string GetString(const string &name, const string &default_value = "") const;

	//! Get any value as string representation (works for int, bool, string, etc.)
	string GetValueAsString(const string &name, const string &default_value = "") const;

	//! Get int field (default if not found)
	int64_t GetInt(const string &name, int64_t default_value = 0) const;

	//! Get bool field (default if not found)
	bool GetBool(const string &name, bool default_value = false) const;

	//! Check if a field exists and has a JSON null value
	bool IsNull(const string &name) const;

	//! Get raw JSON string of a nested object
	string GetObjectAsJSON(const string &name) const;

	//! Validate required fields exist
	bool ValidateRequired(const vector<string> &required_fields) const;

	//! Get list of all field names
	vector<string> GetFieldNames() const;

	//! Destructor cleans up parsed doc
	~JSONArgumentParser();

private:
	yyjson_doc *doc = nullptr;
	yyjson_val *root = nullptr;
	// For STRUCT input, we convert to JSON internally
	string json_buffer;
};

} // namespace duckdb
