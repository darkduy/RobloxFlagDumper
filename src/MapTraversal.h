#pragma once
#include "Types.h"
#include <vector>
#include <cstdint>

class MemoryReader;
struct Config;

// ─────────────────────────── MAP TRAVERSAL ───────────────────────────────
bool TraverseMap(
    const MemoryReader& mem,
    uintptr_t mapAddr,
    std::vector<FlagEntry>& out,
    const Config& cfg);

// Read flag value from node
FlagValue ReadFlagValue(const MemoryReader& mem, uintptr_t nodeAddr, const std::string& name);
