#include "StringScanner.h"
#include "MemoryReader.h"
#include "Config.h"
#include "PESection.h"
#include "Utilities.h"
#include <algorithm>
#include <cstring>

std::vector<FlagRef> ScanFlagStrings(
    const MemoryReader& mem,
    const std::vector<Section>& sections,
    const Config& cfg)
{
    std::vector<FlagRef> refs;
    uint8_t buf[0x1000];

    for (const auto& sec : sections) {
        // Strings usually in .rdata (read-only data)
        if (sec.chars & kSecCode) continue;

        for (uintptr_t addr = sec.va; addr < sec.va + sec.size; addr += 0x1000 - 8) {
            if (!mem.Read(addr, buf, sizeof(buf))) continue;

            for (size_t i = 0; i + 8 <= sizeof(buf); i++) {
                for (const auto& prefix : cfg.flagPrefixes) {
                    size_t plen = prefix.size();
                    if (i + plen > sizeof(buf)) continue;
                    if (memcmp(buf + i, prefix.c_str(), plen) != 0) continue;

                    // Measure string length
                    size_t len = 0;
                    while (i + len < sizeof(buf) && buf[i + len] != '\0') len++;
                    if (len < plen || len >= cfg.maxStrLen) continue;

                    std::string name((char*)buf + i, len);
                    if (!IsPrintableAscii(name)) continue;

                    refs.push_back({ addr + i, name });
                    i += len;
                    break;
                }
            }
        }
    }

    // Remove duplicates (same name)
    std::sort(refs.begin(), refs.end(), [](const FlagRef& a, const FlagRef& b) {
        return a.name < b.name;
    });
    refs.erase(std::unique(refs.begin(), refs.end(), [](const FlagRef& a, const FlagRef& b) {
        return a.name == b.name;
    }), refs.end());

    return refs;
}
