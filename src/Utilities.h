#pragma once
#include <string>
#include <cstdint>

// ─────────────────────────────── UTILITIES ───────────────────────────────
std::string ToHex(uintptr_t v);
bool IsPrime(size_t n);
bool IsPrintableAscii(const std::string& s);

// PE Section constants
static constexpr uint32_t kSecCode = 0x00000020 | 0x20000000;  // IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE
static constexpr uint32_t kSecData = 0x00000040;               // IMAGE_SCN_CNT_INITIALIZED_DATA
