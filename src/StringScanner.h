#pragma once
#include "Types.h"
#include <vector>

class MemoryReader;
struct Config;
struct Section;

// ─────────────────────────── FLAG STRING SCAN ────────────────────────────
std::vector<FlagRef> ScanFlagStrings(
    const MemoryReader& mem,
    const std::vector<Section>& sections,
    const Config& cfg);
