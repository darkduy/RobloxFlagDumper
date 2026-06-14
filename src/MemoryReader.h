#pragma once
#include <windows.h>
#include <string>
#include <cstdint>

struct Config;

// ─────────────────────────────── MEMORY READER ───────────────────────────
class MemoryReader
{
    HANDLE hProc;
    const Config* pCfg;
public:
    explicit MemoryReader(HANDLE h, const Config* cfg = nullptr);

    bool Read(uintptr_t addr, void* buf, size_t sz) const;

    template<typename T>
    T Read(uintptr_t addr) const
    {
        T v{};
        Read(addr, &v, sizeof(v));
        return v;
    }

    // Đọc std::string MSVC SSO-aware với offsets động từ Config
    std::string ReadMsvcString(uintptr_t addr) const;

    // Đọc C-string tại địa chỉ thô
    std::string ReadCString(uintptr_t addr, size_t maxLen = 128) const;
};
