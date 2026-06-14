#include "PESection.h"
#include "MemoryReader.h"
#include <cstring>

std::vector<Section> ParsePeSections(const MemoryReader& mem, uintptr_t base)
{
    std::vector<Section> secs;
    uint8_t  dosHdr[0x40];
    if (!mem.Read(base, dosHdr, sizeof(dosHdr))) return secs;
    if (dosHdr[0] != 'M' || dosHdr[1] != 'Z') return secs;

    uint32_t peOff = *(uint32_t*)(dosHdr + 0x3C);
    uint8_t  ntHdr[0x108];
    if (!mem.Read(base + peOff, ntHdr, sizeof(ntHdr))) return secs;
    if (ntHdr[0] != 'P' || ntHdr[1] != 'E') return secs;

    uint16_t numSec   = *(uint16_t*)(ntHdr + 6);
    uint16_t optSize  = *(uint16_t*)(ntHdr + 20);
    uintptr_t secHdrOff = base + peOff + 24 + optSize;

    for (uint16_t i = 0; i < numSec; i++) {
        uint8_t sh[40];
        if (!mem.Read(secHdrOff + i * 40, sh, 40)) break;
        Section s;
        s.name  = std::string((char*)sh, strnlen((char*)sh, 8));
        s.va    = base + *(uint32_t*)(sh + 12);
        s.size  = *(uint32_t*)(sh + 16);
        s.chars = *(uint32_t*)(sh + 36);
        secs.push_back(s);
    }
    return secs;
}
