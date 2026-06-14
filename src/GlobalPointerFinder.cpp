#include "GlobalPointerFinder.h"
#include "MemoryReader.h"
#include "Config.h"
#include "PESection.h"
#include "InstructionHelpers.h"
#include <map>
#include <cstring>

GlobalPtrs FindGlobalMapPointers(
    const MemoryReader& mem,
    const std::vector<MapCandidate>& maps,
    const std::vector<Section>& codeSections,
    const Config& cfg)
{
    // votes[ptrAddr] = { mapCandidateIndex, hitCount }
    std::map<uintptr_t, std::pair<size_t,int>> votes;
    uint8_t buf[0x1000];

    std::vector<std::pair<uintptr_t,size_t>> codeRanges;
    for (const auto& s : codeSections)
        if (s.chars & kSecCode) codeRanges.push_back({s.va, s.size});

    for (size_t ri = 0; ri < codeRanges.size(); ri++) {
        uintptr_t rangeStart = codeRanges[ri].first;
        size_t    rangeSize  = codeRanges[ri].second;
        for (uintptr_t addr = rangeStart; addr < rangeStart + rangeSize; addr += 0x1000 - 7) {
            if (!mem.Read(addr, buf, sizeof(buf))) continue;
            for (size_t i = 0; i + 7 <= sizeof(buf); i++) {
                if (!IsMovRcxRip(buf+i) && !IsMovRaxRip(buf+i)) continue;
                uintptr_t ptrAddr = ResolveRip(addr + i, buf + i);
                uintptr_t mapAddr = 0;
                
                if (cfg.is64Bit)
                    mapAddr = mem.Read<uintptr_t>(ptrAddr);
                else
                    mapAddr = (uintptr_t)mem.Read<uint32_t>(ptrAddr);
                    
                for (size_t mi = 0; mi < maps.size(); mi++) {
                    if (maps[mi].address == mapAddr) {
                        votes[ptrAddr].first  = mi;
                        votes[ptrAddr].second++;
                        break;
                    }
                }
            }
        }
    }

    // Most-referenced map → FFlagList
    // Second most-referenced distinct map → FlagToValue
    uintptr_t bestPtr1 = 0, bestPtr2 = 0;
    int maxV1 = 0, maxV2 = 0;
    size_t mapIdx1 = SIZE_MAX;

    typedef std::map<uintptr_t, std::pair<size_t,int>> VoteMap;
    for (VoteMap::iterator it = votes.begin(); it != votes.end(); ++it) {
        uintptr_t ptr = it->first;
        size_t    mi2 = it->second.first;
        int       cnt = it->second.second;
        if (cnt > maxV1) {
            maxV2 = maxV1; bestPtr2 = bestPtr1;
            maxV1 = cnt; bestPtr1 = ptr; mapIdx1 = mi2;
        } else if (cnt > maxV2 && mi2 != mapIdx1) {
            maxV2 = cnt; bestPtr2 = ptr;
        }
    }

    return { bestPtr1, bestPtr2 };
}
