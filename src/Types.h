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

// ─────────────────────────── MAP CANDIDATE ───────────────────────────────
struct MapCandidate {
    uintptr_t address;
    size_t    bucketCount;
    size_t    elementCount;
    uintptr_t bucketArray;
    uintptr_t firstNode;
    float     score;
};

// ─────────────────────────── FLAG ENTRY ──────────────────────────────────
struct FlagEntry {
    std::string name;
    FlagValue   value;
    uintptr_t   nodeAddr;  // địa chỉ node trong process memory
};

// ─────────────────────────── FLAG REFERENCE ──────────────────────────────
struct FlagRef {
    uintptr_t   stringAddr;
    std::string name;
};

// ─────────────────────────── GLOBAL POINTERS ─────────────────────────────
struct GlobalPtrs {
    uintptr_t fflagListPtr   = 0;
    uintptr_t flagToValuePtr = 0;
};

// ─────────────────────────── PE SECTION ──────────────────────────────────
struct Section {
    std::string name;
    uintptr_t   va;       // virtual address (absolute)
    size_t      size;
    uint32_t    chars;    // IMAGE_SCN_*
};
