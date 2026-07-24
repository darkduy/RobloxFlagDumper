#pragma once
#include "Types.h"
#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>

// ─────────────────────────────── OUTPUT WRITERS ──────────────────────────
std::string GetRobloxVersion(HANDLE hProc);

void WriteJson(const std::vector<FlagEntry>& flags, const std::string& path);

void WriteHpp(
    const std::vector<FlagEntry>& flags,
    uintptr_t fflagListPtr,
    uintptr_t getterFunc,
    uintptr_t flagToValue,
    const std::string& version,
    const std::string& path);
