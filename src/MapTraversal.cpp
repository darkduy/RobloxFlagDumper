#include "MapTraversal.h"
#include "MemoryReader.h"
#include "Config.h"
#include <set>

FlagValue ReadFlagValue(const MemoryReader& mem, uintptr_t nodeAddr, const std::string& name)
{
    FlagValue fv;
    // Guess type from name prefix
    bool isBool   = (name.rfind("FFlag",  0) == 0 || name.rfind("DFFlag", 0) == 0);
    bool isInt    = (name.rfind("FInt",   0) == 0 || name.rfind("DFInt",  0) == 0);
    bool isString = (name.rfind("FString",0) == 0 || name.rfind("DFString",0)== 0);

    uintptr_t valOff = nodeAddr + 0x28;

    if (isInt) {
        fv.type   = FlagType::Int;
        fv.intVal = mem.Read<int64_t>(valOff);
    } else if (isString) {
        fv.type   = FlagType::String;
        fv.strVal = mem.ReadMsvcString(valOff);
    } else {
        // Default: bool
        uint8_t b = mem.Read<uint8_t>(valOff);
        fv.type    = FlagType::Bool;
        fv.boolVal = (b != 0);
    }
    (void)isBool;
    (void)isString;
    return fv;
}

bool TraverseMap(
    const MemoryReader& mem,
    uintptr_t mapAddr,
    std::vector<FlagEntry>& out,
    const Config& cfg)
{
    size_t elementCount = mem.Read<size_t>(mapAddr + cfg.offsetSize);
    uintptr_t head = 0;
    
    if (cfg.is64Bit)
        head = mem.Read<uintptr_t>(mapAddr + cfg.offsetHead);
    else
        head = (uintptr_t)mem.Read<uint32_t>(mapAddr + cfg.offsetHead);

    if (elementCount == 0 || elementCount > cfg.maxFlags) return false;
    if (head == 0 || head > (cfg.is64Bit ? 0x7FFFFFFF0000ULL : 0xFFFFFFFFU)) return false;

    uintptr_t cur = head;
    size_t count  = 0;
    std::set<uintptr_t> visited;

    while (cur != 0 && count < elementCount) {
        if (visited.count(cur)) break;   // cycle guard
        visited.insert(cur);

        uintptr_t next = 0;
        if (cfg.is64Bit)
            next = mem.Read<uintptr_t>(cur + cfg.offsetNodeNext);
        else
            next = (uintptr_t)mem.Read<uint32_t>(cur + cfg.offsetNodeNext);

        // Read key (std::string)
        std::string key = mem.ReadMsvcString(cur + cfg.offsetNodeKey);
        if (!key.empty() && key.size() >= 4) {
            FlagValue val = ReadFlagValue(mem, cur + cfg.offsetNodeValue, key);
            out.push_back({key, val, cur});
        }

        cur = next;
        count++;
    }
    return count > 0;
}
