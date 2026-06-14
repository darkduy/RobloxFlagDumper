#pragma once
#include <cstdint>

// ─────────────────────────── INSTRUCTION HELPERS ─────────────────────────
// lea rdx, [rip+disp32]  → 48 8D 15 ?? ?? ?? ??
// lea rcx, [rip+disp32]  → 48 8D 0D ?? ?? ?? ??
// mov rcx, [rip+disp32]  → 48 8B 0D ?? ?? ?? ??
// mov rax, [rip+disp32]  → 48 8B 05 ?? ?? ?? ??

bool IsLeaRdxRip(const uint8_t* b);
bool IsLeaRcxRip(const uint8_t* b);
bool IsMovRcxRip(const uint8_t* b);
bool IsMovRaxRip(const uint8_t* b);

// Resolve RIP-relative addressing: address + 7 + displacement
uintptr_t ResolveRip(uintptr_t instrAddr, const uint8_t* bytes);
