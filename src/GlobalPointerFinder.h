#pragma once
#include "Types.h"
#include <vector>

class MemoryReader;
struct Config;
struct Section;

// ─────────────────────── GLOBAL MAP POINTER FINDER ───────────────────────
// Multi-map voting: count how many instructions reference each map candidate
GlobalPtrs FindGlobalMapPointers(
    const MemoryReader& mem,
    const std::vector<MapCandidate>& maps,
    const std::vector<Section>& codeSections,
    const Config& cfg);
