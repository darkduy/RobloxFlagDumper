#include "Types.h"

std::string FlagValue::ToString() const {
    switch (type) {
        case FlagType::Bool:   return boolVal ? "true" : "false";
        case FlagType::Int:    return std::to_string(intVal);
        case FlagType::String: return "\"" + strVal + "\"";
        default:               return "unknown";
    }
}

std::string FlagValue::TypeName() const {
    switch (type) {
        case FlagType::Bool:   return "bool";
        case FlagType::Int:    return "int";
        case FlagType::String: return "string";
        default:               return "unknown";
    }
}
