# 🧪 Roblox Fast Flag Dumper (Educational)

A **fully dynamic** C++ tool that automatically extracts all active Fast Flags (`FFlag*` / `FLog*`) from a running Roblox client process – **without hardcoded offsets or manual pattern updates**.

> ⚠️ **For educational purposes only** – understanding memory scanning, pattern recognition, and Windows internals.  
> Using this tool to cheat or bypass Roblox security violates [Roblox Terms of Service](https://en.help.roblox.com/hc/en-us/articles/115004647846-Roblox-Terms-of-Use).  
> The author assumes no liability for misuse.

---

## ✨ Features

- 🔍 **Zero manual patterns** – automatically finds the flag container, getter function, and global pointers.
- 🧠 **Heuristic detection** of `std::unordered_map` structures (MSVC layout).
- 📦 **Exports** a clean `RobloxFlags.hpp` header with:
  - `FFlagOffsets` namespace (addresses of `FFlagList`, `ValueGetSet`, `FlagToValue`)
  - `FFlags` namespace (offset table for each flag)
- 🔁 **Survives most Roblox updates** – as long as the basic string and map layout remain similar.
- 💻 **Single-file** – no external dependencies, just C++ and Windows API.

---

## 🚀 How to Use

### 1. Compile the tool

You can compile manually or use the provided **GitHub Action** (see below).

**Manual (MSVC – Developer Command Prompt)**  
```bash
cl /O2 /MT RobloxFlagDumper.cpp /Fe:RobloxFlagDumper.exe /link psapi.lib
```

**Manual (MinGW / GCC)**  
```bash
g++ -O2 -static RobloxFlagDumper.cpp -o RobloxFlagDumper.exe -lpsapi
```

### 2. Run Roblox

Launch any Roblox game and **join a server** (so the client is fully initialized).

### 3. Execute the dumper

**Run as Administrator** (required to read process memory).

```bash
RobloxFlagDumper.exe
```

### 4. Get the output

The tool creates `RobloxFlags.hpp` in the same folder, containing:

```cpp
// Roblox Fast Flags – Auto dumped
// Total flags: 13390
// Dumped at 2026-06-12 22:30:00
#pragma once

namespace FFlagOffsets {
    uintptr_t FFlagList = 0x7FF6A3B4C200;
    uintptr_t ValueGetSet = 0x7FF6A3127A40;
    uintptr_t FlagToValue = 0x7FF6A3B4C2E0;
}

namespace FFlags {
    // FFlagDebug = true
    uintptr_t FFlagDebug = 0x7FF6A3B4C300;
    // FLogNetwork = false
    uintptr_t FLogNetwork = 0x7FF6A3B4C328;
    // ...
}
```

---

## 🤖 GitHub Action (Automated Build)

The repository includes a pre‑configured GitHub workflow (`.github/workflows/build.yml`).  
Every push / PR to `main` automatically builds `RobloxFlagDumper.exe` on **Windows‑latest** using MSVC.

### How to use the Action

1. Fork or clone this repository.
2. Push your changes – the workflow will run.
3. Go to **Actions** → select the workflow → **Artifacts** → download the `.exe`.

**Workflow summary**  
- Runs on `windows-latest`  
- Sets up MSVC environment  
- Cleans previous builds  
- Compiles with `/O2 /MT` and links `psapi.lib`  
- Uploads `RobloxFlagDumper.exe` as an artifact

---

## 🧠 How It Works (Brief)

1. **Find process** – `RobloxPlayerBeta.exe`.
2. **Scan for flag strings** – searches the `.rdata` section for `FFlag*` literals.
3. **Locate getter function** – finds code that references those strings (via `lea rdx, [string]`) and identifies the function containing most references.
4. **Discover `unordered_map` candidates** – memory scan for objects with plausible `{bucket_count, head, bucket_array, size}` fields.
5. **Resolve `FFlagList`** – finds a `mov rcx, [rip+disp]` instruction that points to one of the map candidates.
6. **Traverse the map** – follows the internal linked list of MSVC’s `std::unordered_map`, reads each flag name and value (bool assumed).
7. **Export** – writes `RobloxFlags.hpp` with offsets.

All offsets (map layout, string pattern, instruction signatures) are **determined at runtime** – no manual updates needed.

---

## ⚠️ Limitations & Future Adaptations

- **Map layout** – assumes MSVC’s `std::unordered_map` (node: `next`, `hash`, `key` string, `value` at `+0x28`). If Roblox changes its container, traversal will fail (but the tool will still find the map object – you’d need to adjust node offsets).
- **Flag values** – currently reads only `bool`. Extending to `int`, `float`, or `std::string` requires analysing the actual node layout.
- **String encoding** – searches ASCII `FFlag`. If Roblox obfuscates or encrypts flag names, the dumper will break.

---

## 📚 Educational Value

This tool teaches:

- Windows API memory reading (`ReadProcessMemory`, `VirtualQueryEx`)
- Process enumeration (`Toolhelp32Snapshot`)
- Pattern scanning and heuristic struct detection
- Walking a C++ standard library container (`std::unordered_map`) from an external process
- Reverse engineering concepts without static signatures

---

## 🧾 License

This project is licensed under the **MIT License** – free to use, modify, and distribute for **legal and educational purposes only**.

---

## 🙏 Credits

- Built for the reverse engineering & game security research community.
- Inspired by real‑world dynamic analysis techniques.

---

**Remember:** Understanding how something works is the first step to defending it.  
Use this knowledge responsibly.