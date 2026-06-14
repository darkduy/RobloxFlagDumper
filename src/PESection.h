#pragma once
#include "Types.h"
#include <vector>
#include <cstdint>

class MemoryReader;

// ─────────────────────────────── PE SECTIONS ─────────────────────────────
std::vector<Section> ParsePeSections(const MemoryReader& mem, uintptr_t base);
