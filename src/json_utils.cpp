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
        case LogicalTypeId::BLOB:
            return yyjson_mut_strcpy(doc, value.ToString().c_str());
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

} // namespace duckdb