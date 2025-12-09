//===----------------------------------------------------------------------===//
//                         DuckDB
//
// json_utils.cpp
//
//
//===----------------------------------------------------------------------===//

#include "json_utils.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/types/value.hpp"

namespace duckdb {

yyjson_mut_doc *JSONUtils::CreateDocument() {
    return yyjson_mut_doc_new(nullptr);
}

yyjson_mut_val *JSONUtils::CreateObject(yyjson_mut_doc *doc) {
    if (!doc) {
        throw InternalException("JSON document is null");
    }
    return yyjson_mut_obj(doc);
}

yyjson_mut_val *JSONUtils::CreateArray(yyjson_mut_doc *doc) {
    if (!doc) {
        throw InternalException("JSON document is null");
    }
    return yyjson_mut_arr(doc);
}

void JSONUtils::AddString(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const string &value) {
    if (!doc || !obj || !key) {
        throw InternalException("Invalid parameters for AddString");
    }
    yyjson_mut_val *val = yyjson_mut_strcpy(doc, value.c_str());
    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(doc, key), val);
}

void JSONUtils::AddInt(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, int64_t value) {
    if (!doc || !obj || !key) {
        throw InternalException("Invalid parameters for AddInt");
    }
    yyjson_mut_val *val = yyjson_mut_sint(doc, value);
    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(doc, key), val);
}

void JSONUtils::AddBool(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, bool value) {
    if (!doc || !obj || !key) {
        throw InternalException("Invalid parameters for AddBool");
    }
    yyjson_mut_val *val = yyjson_mut_bool(doc, value);
    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(doc, key), val);
}

void JSONUtils::AddNull(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key) {
    if (!doc || !obj || !key) {
        throw InternalException("Invalid parameters for AddNull");
    }
    yyjson_mut_val *val = yyjson_mut_null(doc);
    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(doc, key), val);
}

void JSONUtils::AddObject(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key, yyjson_mut_val *child) {
    if (!doc || !parent || !key || !child) {
        throw InternalException("Invalid parameters for AddObject");
    }
    yyjson_mut_obj_add(parent, yyjson_mut_strcpy(doc, key), child);
}

void JSONUtils::AddArray(yyjson_mut_doc *doc, yyjson_mut_val *parent, const char *key, yyjson_mut_val *child) {
    if (!doc || !parent || !key || !child) {
        throw InternalException("Invalid parameters for AddArray");
    }
    yyjson_mut_obj_add(parent, yyjson_mut_strcpy(doc, key), child);
}

void JSONUtils::ArrayAdd(yyjson_mut_doc *doc, yyjson_mut_val *arr, yyjson_mut_val *val) {
    if (!doc || !arr || !val) {
        throw InternalException("Invalid parameters for ArrayAdd");
    }
    yyjson_mut_arr_add_val(arr, val);
}

void JSONUtils::ArrayAddString(yyjson_mut_doc *doc, yyjson_mut_val *arr, const string &value) {
    if (!doc || !arr) {
        throw InternalException("Invalid parameters for ArrayAddString");
    }
    yyjson_mut_val *val = yyjson_mut_strcpy(doc, value.c_str());
    yyjson_mut_arr_add_val(arr, val);
}

string JSONUtils::Serialize(yyjson_mut_doc *doc) {
    if (!doc) {
        throw InternalException("JSON document is null");
    }
    
    size_t len;
    char *json = yyjson_mut_write(doc, 0, &len);
    if (!json) {
        throw InternalException("Failed to serialize JSON document");
    }
    
    string result(json, len);
    free(json);
    return result;
}

yyjson_doc *JSONUtils::Parse(const string &json) {
    yyjson_doc *doc = yyjson_read(json.c_str(), json.length(), 0);
    if (!doc) {
        throw InvalidInputException("Failed to parse JSON: %s", json.c_str());
    }
    return doc;
}

void JSONUtils::FreeDocument(yyjson_mut_doc *doc) {
    if (doc) {
        yyjson_mut_doc_free(doc);
    }
}

void JSONUtils::FreeDocument(yyjson_doc *doc) {
    if (doc) {
        yyjson_doc_free(doc);
    }
}

yyjson_mut_val *JSONUtils::ValueToJSON(yyjson_mut_doc *doc, const Value &value) {
    if (!doc) {
        throw InternalException("JSON document is null");
    }

    if (value.IsNull()) {
        return yyjson_mut_null(doc);
    }

    switch (value.type().id()) {
        case LogicalTypeId::BOOLEAN:
            return yyjson_mut_bool(doc, value.GetValue<bool>());
        case LogicalTypeId::TINYINT:
            return yyjson_mut_sint(doc, value.GetValue<int8_t>());
        case LogicalTypeId::SMALLINT:
            return yyjson_mut_sint(doc, value.GetValue<int16_t>());
        case LogicalTypeId::INTEGER:
            return yyjson_mut_sint(doc, value.GetValue<int32_t>());
        case LogicalTypeId::BIGINT:
            return yyjson_mut_sint(doc, value.GetValue<int64_t>());
        case LogicalTypeId::UTINYINT:
            return yyjson_mut_uint(doc, value.GetValue<uint8_t>());
        case LogicalTypeId::USMALLINT:
            return yyjson_mut_uint(doc, value.GetValue<uint16_t>());
        case LogicalTypeId::UINTEGER:
            return yyjson_mut_uint(doc, value.GetValue<uint32_t>());
        case LogicalTypeId::UBIGINT:
            return yyjson_mut_uint(doc, value.GetValue<uint64_t>());
        case LogicalTypeId::FLOAT:
            return yyjson_mut_real(doc, value.GetValue<float>());
        case LogicalTypeId::DOUBLE:
            return yyjson_mut_real(doc, value.GetValue<double>());
        case LogicalTypeId::VARCHAR:
        case LogicalTypeId::BLOB: {
            // Check if this is a JSON-typed value - if so, parse and inline it
            // rather than escaping it as a string
            if (value.type().id() == LogicalTypeId::VARCHAR &&
                value.type().GetAlias() == "JSON") {
                string json_str = value.ToString();
                if (!json_str.empty()) {
                    // Parse the JSON string
                    yyjson_doc *parsed = yyjson_read(json_str.c_str(), json_str.length(), 0);
                    if (parsed) {
                        // Copy the parsed value into our mutable document
                        yyjson_val *root = yyjson_doc_get_root(parsed);
                        yyjson_mut_val *copied = yyjson_val_mut_copy(doc, root);
                        yyjson_doc_free(parsed);
                        return copied;
                    }
                }
            }
            // Regular string - escape as usual
            return yyjson_mut_strcpy(doc, value.ToString().c_str());
        }
        case LogicalTypeId::STRUCT: {
            // Convert STRUCT to JSON object
            yyjson_mut_val *obj = CreateObject(doc);
            auto &struct_values = StructValue::GetChildren(value);
            auto &struct_type = value.type();
            for (size_t i = 0; i < struct_values.size(); i++) {
                auto &key = StructType::GetChildName(struct_type, i);
                yyjson_mut_val *child_val = ValueToJSON(doc, struct_values[i]);
                yyjson_mut_obj_add(obj, yyjson_mut_strcpy(doc, key.c_str()), child_val);
            }
            return obj;
        }
        case LogicalTypeId::LIST: {
            // Convert LIST to JSON array
            yyjson_mut_val *arr = CreateArray(doc);
            auto &list_values = ListValue::GetChildren(value);
            for (auto &item : list_values) {
                yyjson_mut_val *item_val = ValueToJSON(doc, item);
                ArrayAdd(doc, arr, item_val);
            }
            return arr;
        }
        case LogicalTypeId::MAP: {
            // Convert MAP to JSON object (keys must be strings for valid JSON)
            yyjson_mut_val *obj = CreateObject(doc);
            auto &map_values = MapValue::GetChildren(value);
            for (auto &kv : map_values) {
                auto &kv_children = StructValue::GetChildren(kv);
                if (kv_children.size() >= 2) {
                    string key = kv_children[0].ToString();
                    yyjson_mut_val *val = ValueToJSON(doc, kv_children[1]);
                    yyjson_mut_obj_add(obj, yyjson_mut_strcpy(doc, key.c_str()), val);
                }
            }
            return obj;
        }
        default:
            // For other types, convert to string
            return yyjson_mut_strcpy(doc, value.ToString().c_str());
    }
}

yyjson_mut_val *JSONUtils::QueryResultToJSON(yyjson_mut_doc *doc, const unique_ptr<QueryResult> &result) {
    if (!doc) {
        throw InternalException("JSON document is null");
    }
    
    if (!result || result->HasError()) {
        return yyjson_mut_null(doc);
    }
    
    yyjson_mut_val *json_array = CreateArray(doc);
    
    // Process each chunk
    while (true) {
        auto chunk = result->Fetch();
        if (!chunk || chunk->size() == 0) {
            break;
        }
        
        for (idx_t row = 0; row < chunk->size(); row++) {
            yyjson_mut_val *json_row = CreateObject(doc);
            
            for (idx_t col = 0; col < result->names.size(); col++) {
                const string &column_name = result->names[col];
                Value cell_value = chunk->GetValue(col, row);
                
                yyjson_mut_val *json_value = ValueToJSON(doc, cell_value);
                yyjson_mut_obj_add(json_row, yyjson_mut_strcpy(doc, column_name.c_str()), json_value);
            }
            
            ArrayAdd(doc, json_array, json_row);
        }
    }
    
    return json_array;
}

yyjson_mut_val *JSONUtils::CreateMCPMessage(yyjson_mut_doc *doc, const char *jsonrpc) {
    if (!doc || !jsonrpc) {
        throw InternalException("Invalid parameters for CreateMCPMessage");
    }
    
    yyjson_mut_val *msg = CreateObject(doc);
    AddString(doc, msg, "jsonrpc", jsonrpc);
    return msg;
}

string JSONUtils::GetString(yyjson_val *obj, const char *key, const string &default_value) {
    if (!obj || !key) {
        return default_value;
    }
    
    yyjson_val *val = yyjson_obj_get(obj, key);
    if (!val || !yyjson_is_str(val)) {
        return default_value;
    }
    
    const char *str = yyjson_get_str(val);
    return str ? string(str) : default_value;
}

int64_t JSONUtils::GetInt(yyjson_val *obj, const char *key, int64_t default_value) {
    if (!obj || !key) {
        return default_value;
    }
    
    yyjson_val *val = yyjson_obj_get(obj, key);
    if (!val || !yyjson_is_int(val)) {
        return default_value;
    }
    
    return yyjson_get_sint(val);
}

bool JSONUtils::GetBool(yyjson_val *obj, const char *key, bool default_value) {
    if (!obj || !key) {
        return default_value;
    }
    
    yyjson_val *val = yyjson_obj_get(obj, key);
    if (!val || !yyjson_is_bool(val)) {
        return default_value;
    }
    
    return yyjson_get_bool(val);
}

yyjson_val *JSONUtils::GetObject(yyjson_val *obj, const char *key) {
    if (!obj || !key) {
        return nullptr;
    }
    
    yyjson_val *val = yyjson_obj_get(obj, key);
    if (!val || !yyjson_is_obj(val)) {
        return nullptr;
    }
    
    return val;
}

yyjson_val *JSONUtils::GetArray(yyjson_val *obj, const char *key) {
    if (!obj || !key) {
        return nullptr;
    }

    yyjson_val *val = yyjson_obj_get(obj, key);
    if (!val || !yyjson_is_arr(val)) {
        return nullptr;
    }

    return val;
}

//===--------------------------------------------------------------------===//
// JSONArgumentParser Implementation
//===--------------------------------------------------------------------===//

bool JSONArgumentParser::Parse(const Value &arguments) {
    // Clean up any previous state
    if (doc) {
        JSONUtils::FreeDocument(doc);
        doc = nullptr;
        root = nullptr;
    }

    if (arguments.IsNull()) {
        // Empty arguments - create empty JSON object
        json_buffer = "{}";
        doc = JSONUtils::Parse(json_buffer);
        root = yyjson_doc_get_root(doc);
        return true;
    }

    if (arguments.type().id() == LogicalTypeId::VARCHAR) {
        // Already a JSON string - parse directly
        json_buffer = arguments.ToString();
        if (json_buffer.empty()) {
            json_buffer = "{}";
        }
        try {
            doc = JSONUtils::Parse(json_buffer);
            root = yyjson_doc_get_root(doc);
            return root != nullptr && yyjson_is_obj(root);
        } catch (...) {
            return false;
        }
    } else if (arguments.type().id() == LogicalTypeId::STRUCT) {
        // Convert STRUCT to JSON string first, then parse
        yyjson_mut_doc *mut_doc = JSONUtils::CreateDocument();
        yyjson_mut_val *json_obj = JSONUtils::ValueToJSON(mut_doc, arguments);
        yyjson_mut_doc_set_root(mut_doc, json_obj);
        json_buffer = JSONUtils::Serialize(mut_doc);
        JSONUtils::FreeDocument(mut_doc);

        // Now parse the JSON string
        doc = JSONUtils::Parse(json_buffer);
        root = yyjson_doc_get_root(doc);
        return root != nullptr && yyjson_is_obj(root);
    }

    // Unsupported argument type
    return false;
}

bool JSONArgumentParser::HasField(const string &name) const {
    if (!root) {
        return false;
    }
    yyjson_val *val = yyjson_obj_get(root, name.c_str());
    return val != nullptr;
}

string JSONArgumentParser::GetString(const string &name, const string &default_value) const {
    if (!root) {
        return default_value;
    }
    return JSONUtils::GetString(root, name.c_str(), default_value);
}

int64_t JSONArgumentParser::GetInt(const string &name, int64_t default_value) const {
    if (!root) {
        return default_value;
    }
    return JSONUtils::GetInt(root, name.c_str(), default_value);
}

bool JSONArgumentParser::GetBool(const string &name, bool default_value) const {
    if (!root) {
        return default_value;
    }
    return JSONUtils::GetBool(root, name.c_str(), default_value);
}

string JSONArgumentParser::GetObjectAsJSON(const string &name) const {
    if (!root) {
        return "{}";
    }
    yyjson_val *obj = JSONUtils::GetObject(root, name.c_str());
    if (!obj) {
        return "{}";
    }
    size_t len;
    char *json = yyjson_val_write(obj, 0, &len);
    if (!json) {
        return "{}";
    }
    string result(json, len);
    free(json);
    return result;
}

bool JSONArgumentParser::ValidateRequired(const vector<string> &required_fields) const {
    for (const auto &field : required_fields) {
        if (!HasField(field)) {
            return false;
        }
    }
    return true;
}

vector<string> JSONArgumentParser::GetFieldNames() const {
    vector<string> names;
    if (!root || !yyjson_is_obj(root)) {
        return names;
    }

    yyjson_obj_iter iter;
    yyjson_obj_iter_init(root, &iter);
    yyjson_val *key;
    while ((key = yyjson_obj_iter_next(&iter)) != nullptr) {
        const char *key_str = yyjson_get_str(key);
        if (key_str) {
            names.push_back(string(key_str));
        }
    }
    return names;
}

JSONArgumentParser::~JSONArgumentParser() {
    if (doc) {
        JSONUtils::FreeDocument(doc);
    }
}

} // namespace duckdb