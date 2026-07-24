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

// Read flag value at an absolute address (not relative to node start).
FlagValue ReadFlagValue(const MemoryReader& mem, uintptr_t valueAddr, const std::string& name);
