#include "InstructionHelpers.h"
#include <cstring>

bool IsLeaRdxRip(const uint8_t* b) { return b[0]==0x48 && b[1]==0x8D && b[2]==0x15; }
bool IsLeaRcxRip(const uint8_t* b) { return b[0]==0x48 && b[1]==0x8D && b[2]==0x0D; }
bool IsMovRcxRip(const uint8_t* b) { return b[0]==0x48 && b[1]==0x8B && b[2]==0x0D; }
bool IsMovRaxRip(const uint8_t* b) { return b[0]==0x48 && b[1]==0x8B && b[2]==0x05; }

uintptr_t ResolveRip(uintptr_t instrAddr, const uint8_t* bytes)
{
    int32_t disp = *(int32_t*)(bytes + 3);
    return instrAddr + 7 + disp;
}
