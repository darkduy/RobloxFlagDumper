#pragma once
#include "Types.h"
#include <vector>
#include <cstdint>

class MemoryReader;
struct Config;
struct Section;

// ─────────────────────────── GETTER FUNCTION FINDER ──────────────────────
uintptr_t FindGetterFunction(
    const MemoryReader& mem,
    const std::vector<FlagRef>& flagRefs,
    const std::vector<Section>& codeSections,
    const Config& cfg);
