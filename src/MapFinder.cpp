#include "MapFinder.h"
#include "MemoryReader.h"
#include "Config.h"
#include "PESection.h"
#include "Utilities.h"
#include <algorithm>
#include <cstring>
#include <cmath>

std::vector<MapCandidate> FindMapCandidates(
    const MemoryReader& mem,
    const std::vector<Section>& sections,
    const Config& cfg)
{
    std::vector<MapCandidate> out;

    for (const auto& sec : sections) {
        // Only search in data sections, not code
        if (sec.chars & kSecCode) continue;
        if (!(sec.chars & kSecData)) continue;

        uint8_t buf[0x1000];
        for (uintptr_t addr = sec.va; addr < sec.va + sec.size; addr += 0x1000 - 0x30) {
            if (!mem.Read(addr, buf, sizeof(buf))) continue;

            for (size_t off = 0; off + 0x28 <= sizeof(buf); off += 8) {
                uintptr_t objAddr = addr + off;
                
                size_t bucketCount = 0, elementCount = 0;
                uintptr_t firstNode = 0, bucketArr = 0;
                uint32_t loadFactor = 0;
                
                if (cfg.is64Bit) {
                    bucketCount = *(size_t*)(buf + off + cfg.offsetBucketCount);
                    firstNode   = *(uintptr_t*)(buf + off + cfg.offsetHead);
                    bucketArr   = *(uintptr_t*)(buf + off + cfg.offsetBucketArray);
                    elementCount = *(size_t*)(buf + off + cfg.offsetSize);
                    loadFactor  = *(uint32_t*)(buf + off + cfg.offsetMaxLoadFactor);
                } else {
                    bucketCount = *(size_t*)(buf + off + cfg.offsetBucketCount);
                    firstNode   = (uintptr_t)*(uint32_t*)(buf + off + cfg.offsetHead);
                    bucketArr   = (uintptr_t)*(uint32_t*)(buf + off + cfg.offsetBucketArray);
                    elementCount = *(size_t*)(buf + off + cfg.offsetSize);
                    loadFactor  = *(uint32_t*)(buf + off + cfg.offsetMaxLoadFactor);
                }

                // Quick filter
                if (elementCount < cfg.minElements || elementCount > cfg.maxFlags) continue;
                if (bucketCount  < elementCount || bucketCount > cfg.maxBuckets) continue;
                if (bucketArr < 0x10000 || bucketArr > (cfg.is64Bit ? 0x7FFFFFFF0000ULL : 0xFFFFFFFFU)) continue;

                float score = 0.0f;

                // Load factor == 1.0f (IEEE 754)
                if (loadFactor == cfg.loadFactorIEEE) score += 20.0f;

                // Bucket count is prime (MSVC prefers prime bucket counts)
                if (IsPrime(bucketCount)) score += 15.0f;

                // Larger element counts are prioritized
                if (elementCount > 5000)  score += 10.0f;
                if (elementCount > 10000) score += 10.0f;

                // Valid first node pointer
                if (firstNode > 0x10000 && firstNode < (cfg.is64Bit ? 0x7FFFFFFF0000ULL : 0xFFFFFFFFU)) score += 10.0f;

                // Reasonable load factor
                float lf = (float)elementCount / (float)bucketCount;
                if (lf > 0.3f && lf <= 1.0f) score += 5.0f;

                out.push_back({objAddr, bucketCount, elementCount, bucketArr, firstNode, score});
            }
        }
    }

    // Sort by score (highest first)
    std::sort(out.begin(), out.end(), [](const MapCandidate& a, const MapCandidate& b) {
        return a.score > b.score;
    });

    // Deduplicate nearby addresses
    std::vector<MapCandidate> dedup;
    for (const auto& c : out) {
        bool dup = false;
        for (const auto& d : dedup)
            if (std::abs((intptr_t)(c.address - d.address)) < 0x100) { dup = true; break; }
        if (!dup) dedup.push_back(c);
    }
    return dedup;
}
