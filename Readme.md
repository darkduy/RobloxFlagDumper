# Roblox Fast Flag Dumper

A fully dynamic C++ tool that extracts all active Fast Flags (`FFlag*` / `DFFlag*` / `FInt*` / `FString*`) from a running Roblox client — **no hardcoded offsets, no manual pattern updates**.

> ⚠️ **Educational purposes only.**  
> Using this tool to cheat or bypass Roblox security violates the [Roblox Terms of Service](https://en.help.roblox.com/hc/en-us/articles/115004647846-Roblox-Terms-of-Use).  
> The author assumes no liability for misuse.

---

## Features

- **Zero hardcoded offsets** — finds `FFlagList`, `ValueGetSet`, and `FlagToValue` dynamically at runtime.
- **PE section-aware scanning** — searches `.rdata` for flag strings, `.text` for code references, `.data` for map objects; no wasted cross-section scans.
- **Multi-map voting** — counts how many instructions reference each map candidate; the two most-referenced distinct maps become `FFlagList` and `FlagToValue`.
- **SSO-aware string reader** — handles both MSVC short-string-optimized (inline ≤ 15 chars) and heap-allocated `std::string` keys.
- **Prime-bucket heuristic** — MSVC `std::unordered_map` uses prime bucket counts; scoring rewards this, reducing false positives.
- **Value type detection** — infers `bool` / `int64` / `string` from the flag name prefix (`FFlag` → bool, `FInt` → int64, `FString` → string).
- **Roblox version in header** — reads `FileVersionInfo` from the process EXE and writes it into the output.
- **Single-file, no dependencies** — only Windows API (`psapi`, `version`).

---

## Output

`RobloxFlags.hpp` — written next to the executable:

```cpp
// Roblox Version - version-0-663-0-12345
// Total flags: 13390
// Dumped by RobloxFlagDumper at 2026-06-12 22:30:00
#pragma once
namespace FFlagOffsets {
    uintptr_t FFlagList   = 0x7FF6A3B4C200;
    uintptr_t ValueGetSet = 0x7FF6A3127A40;
    uintptr_t FlagToValue = 0x7FF6A3B4C2E0;
}
namespace FFlags {
    uintptr_t FFlagDebugDisplayFPS = 0x7FF6AB12C340;
    uintptr_t FIntRenderingQuality = 0x7FF6AB12D080;
    ...
}
```

Each entry is the **node address** inside the process — the actual memory location of that flag's map node.

---

## How to Use

### 1. Compile

**MSVC (Developer Command Prompt)**
```
cl /O2 /MT /Fe:RobloxFlagDumper.exe RobloxFlagDumper.cpp /link psapi.lib version.lib
```

**MinGW / GCC**
```
g++ -O2 -static RobloxFlagDumper.cpp -o RobloxFlagDumper.exe -lpsapi -lversion
```

### 2. Run Roblox

Launch any Roblox game and join a server so the client is fully initialized.

### 3. Execute

Run as **Administrator** (required to open the process for reading):

```
RobloxFlagDumper.exe
```

---

## GitHub Action

The repo includes `.github/workflows/build.yml`. Every push to `main` automatically builds on `windows-latest` using MSVC and uploads `RobloxFlagDumper.exe` as an artifact.

**Steps:** fork → push → Actions → select run → Artifacts → download.

---

## How It Works

| Step | What happens |
|------|--------------|
| 1 | Find `RobloxPlayerBeta.exe` via `Toolhelp32Snapshot` |
| 2 | Read base address + `SizeOfImage` via `EnumProcessModules` / `GetModuleInformation` |
| 3 | Parse the PE header to enumerate sections (`.text`, `.rdata`, `.data`, …) |
| 4 | Scan `.rdata` for byte sequences starting with `FFlag`, `DFFlag`, `FInt`, `FString` |
| 5 | Scan `.text` for `lea rdx/rcx, [rip+disp]` instructions that point to those strings; walk backwards to find function prologues → highest-hit function = `ValueGetSet` |
| 6 | Scan `.data` sections for MSVC `unordered_map` layout `{bucket_count, head, bucket_array, size, load_factor}`; score candidates by prime bucket count, load factor = 1.0, element count |
| 7 | Scan `.text` for `mov rcx/rax, [rip+disp]` whose target dereferences to a map candidate; count votes per pointer → top two distinct maps = `FFlagList` + `FlagToValue` |
| 8 | Follow the internal linked list of the winning map; read each node's `std::string` key and value |
| 9 | Read `FileVersionInfo` from the EXE for the version string |
| 10 | Write `RobloxFlags.hpp` |

All offsets are determined at runtime — the tool adapts automatically after most Roblox updates.

---

## Limitations

| Limitation | Detail |
|------------|--------|
| Container layout | Assumes MSVC `std::unordered_map` node layout. If Roblox switches containers, traversal fails (the map object will still be found; only node offsets need adjusting). |
| String encoding | Searches plain ASCII `FFlag*`. Obfuscated or encrypted flag names will not be found. |
| Value offset | `value` field assumed at node `+0x28` (standard MSVC layout). Changes here require a small constant update. |

---

## Educational Value

- `ReadProcessMemory` / `VirtualQueryEx` — Windows memory reading API
- `Toolhelp32Snapshot` — process and module enumeration
- PE header parsing — section table, RVA resolution
- Pattern scanning — byte-level instruction recognition without a disassembler
- Heuristic struct detection — finding C++ stdlib objects from the outside
- RIP-relative addressing — how x64 compilers encode global variable accesses
- Walking `std::unordered_map` externally — following MSVC's linked-list node chain

---

## License

MIT — free to use, modify, and distribute for **legal and educational purposes only**.
