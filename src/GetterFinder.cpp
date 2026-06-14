#include "GetterFinder.h"
#include "MemoryReader.h"
#include "Config.h"
#include "PESection.h"
#include "InstructionHelpers.h"
#include "Logger.h"
#include <map>
#include <cstring>

uintptr_t FindGetterFunction(
    const MemoryReader& mem,
    const std::vector<FlagRef>& flagRefs,
    const std::vector<Section>& codeSections,
    const Config& cfg)
{
    // Get code sections
    std::vector<std::pair<uintptr_t,size_t>> codeRanges;
    for (const auto& s : codeSections)
        if (s.chars & kSecCode)
            codeRanges.push_back({s.va, s.size});

    std::map<uintptr_t, int> funcHits;
    uint8_t buf[0x1000];

    size_t total = flagRefs.size();
    size_t done  = 0;

    for (const auto& ref : flagRefs) {
        done++;
        if (done % 50 == 0 || done == total)
            Log::Progress(done, total, "flag strings");

        for (size_t ri = 0; ri < codeRanges.size(); ri++) {
            uintptr_t rangeStart = codeRanges[ri].first;
            size_t    rangeSize  = codeRanges[ri].second;
            for (uintptr_t addr = rangeStart; addr < rangeStart + rangeSize; addr += 0x1000 - 7) {
                if (!mem.Read(addr, buf, sizeof(buf))) continue;
                for (size_t i = 0; i + 7 <= sizeof(buf); i++) {
                    // lea rdx, [rip+disp] or lea rcx, [rip+disp]
                    if (!IsLeaRdxRip(buf+i) && !IsLeaRcxRip(buf+i)) continue;
                    uintptr_t target = ResolveRip(addr + i, buf + i);
                    if (target != ref.stringAddr) continue;

                    // Try to find prologue backwards (max 512 bytes)
                    uintptr_t instrAddr = addr + i;
                    for (uintptr_t scan = instrAddr - 2; scan > instrAddr - 512; scan--) {
                        uint8_t prologue[6];
                        if (!mem.Read(scan, prologue, 6)) continue;
                        // Common x64 MSVC prologue: 40 53 / 41 54 / 48 83 EC
                        bool isPrologue =
                            (prologue[0] == 0x40 && (prologue[1] == 0x53 || prologue[1] == 0x55)) ||
                            (prologue[0] == 0x41 && (prologue[1] == 0x54 || prologue[1] == 0x56)) ||
                            (prologue[0] == 0x48 && prologue[1] == 0x83 && prologue[2] == 0xEC);
                        if (isPrologue) {
                            funcHits[scan]++;
                            break;
                        }
                        // Stop if INT3 or RET
                        if (prologue[0] == 0xCC || prologue[0] == 0xC3) break;
                    }
                }
            }
        }
    }
    Log::ProgressDone("flag strings");

    uintptr_t best = 0; int maxHits = 0;
    for (std::map<uintptr_t,int>::iterator it = funcHits.begin(); it != funcHits.end(); ++it)
        if (it->second > maxHits) { maxHits = it->second; best = it->first; }
    return best;
}
