#pragma once
#include "Types.h"
#include <vector>

class MemoryReader;
struct Config;
struct Section;

// ─────────────────────────── MAP CANDIDATE FINDER ─────────────────────────
std::vector<MapCandidate> FindMapCandidates(
    const MemoryReader& mem,
    const std::vector<Section>& sections,
    const Config& cfg);
