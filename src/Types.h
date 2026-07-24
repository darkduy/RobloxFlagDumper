#pragma once
#include <string>
#include <cstdint>

// ─────────────────────────────── FLAG VALUE TYPE ──────────────────────────
enum class FlagType { Bool, Int, String, Unknown };

struct FlagValue {
    FlagType    type;
    bool        boolVal;
    int64_t     intVal;
    std::string strVal;

    std::string ToString() const;
    std::string TypeName() const;
};

// ─────────────────────────── FLAG ENTRY ──────────────────────────────────
struct FlagEntry {
    std::string name;
    FlagValue   value;
    uintptr_t   nodeAddr;  // địa chỉ node trong process memory
};