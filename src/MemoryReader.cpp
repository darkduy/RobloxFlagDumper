#include "MemoryReader.h"
#include "Config.h"
#include <cstring>
#include <cctype>

MemoryReader::MemoryReader(HANDLE h, const Config* cfg)
    : hProc(h), pCfg(cfg)
{
}

bool MemoryReader::Read(uintptr_t addr, void* buf, size_t sz) const
{
    SIZE_T got;
    return ReadProcessMemory(hProc, (LPCVOID)addr, buf, sz, &got) && got == sz;
}

static bool IsPrintableAscii(const std::string& s)
{
    for (unsigned char c : s)
        if (c < 0x20 || c > 0x7E) return false;
    return true;
}

std::string MemoryReader::ReadMsvcString(uintptr_t addr) const
{
    if (!pCfg) return {};
    
    size_t sz  = Read<size_t>(addr + pCfg->offsetStringSize);
    size_t res = Read<size_t>(addr + pCfg->offsetStringRes);
    if (sz == 0 || sz > pCfg->maxStrLen) return {};

    char buf[256]{};
    if (res < (size_t)pCfg->ssoThreshold) {
        // inline storage
        Read(addr + pCfg->offsetStringPtr, buf, sz < 255 ? sz : 255);
    } else {
        uintptr_t ptr = pCfg->is64Bit ? Read<uintptr_t>(addr + pCfg->offsetStringPtr)
                                       : (uintptr_t)Read<uint32_t>(addr + pCfg->offsetStringPtr);
        if (!ptr || ptr < 0x10000) return {};
        if (!Read(ptr, buf, sz > 255 ? 255 : sz)) return {};
    }
    buf[sz < 255 ? sz : 255] = '\0';
    std::string s(buf, sz);
    return IsPrintableAscii(s) ? s : std::string{};
}

std::string MemoryReader::ReadCString(uintptr_t addr, size_t maxLen) const
{
    char buf[256]{};
    maxLen = maxLen > 255 ? 255 : maxLen;
    if (!Read(addr, buf, maxLen)) return {};
    buf[maxLen] = '\0';
    size_t len = strnlen(buf, maxLen);
    std::string s(buf, len);
    return IsPrintableAscii(s) ? s : std::string{};
}
