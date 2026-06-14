# Roblox Fast Flag Dumper v3.0 (Dynamic Edition)

**A fully dynamic, version-aware C++ tool that extracts all active Fast Flags from a running Roblox client.**

✨ **Now with configurable offsets, automatic version detection, and 32-bit support!**

> ⚠️ **Educational purposes only.**  
> Using this tool to cheat or bypass Roblox security violates the [Roblox Terms of Service](https://en.help.roblox.com/hc/en-us/articles/115004647846-Roblox-Terms-of-Use).  
> The author assumes no liability for misuse.

---

## What's New in v3.0 (Dynamic)

### 1. **Configuration System**
- Load offset configurations from `roblox_config.json` (optional)
- Automatic detection of 32-bit vs 64-bit processes
- Version-specific offset mappings for different Roblox versions
- Sensible defaults for MSVC x64/x86 memory layouts

### 2. **Version Detection & Adaptation**
- Automatically detects Roblox version from process EXE
- Loads version-specific offsets (extensible framework)
- Includes fallback defaults that work for most MSVC builds

### 3. **32-bit Support**
- Detects process architecture using `IsWow64Process()`
- Adjusts all pointer sizes and struct offsets for 32-bit processes
- Uses separate offset configurations for x86 vs x64

### 4. **Dynamic Offset Discovery**
- All hardcoded offsets are now configuration-driven
- `Config` struct holds all memory layout information
- Easy to extend for non-MSVC compilers or custom layouts

### 5. **Multiple Process Detection**
- Scans for both `RobloxPlayerBeta.exe` and `RobloxPlayer.exe`
- Extensible process name list in `Config::processNames`

---

## Configuration Structure

### Default In-Memory Config (No JSON Needed)

By default, the program uses hardcoded defaults that work for most Roblox versions:

**64-bit MSVC (x64):**
```
std::unordered_map<std::string, T> offsets:
  +0x00  size_t _Bucket_count
  +0x08  _Node* _Head
  +0x10  _Bucket* _Buckets
  +0x18  size_t _Size
  +0x20  float _Max_load_factor

std::string offsets (SSO-aware):
  +0x00  char* _Ptr (or inline buffer)
  +0x10  size_t _Size
  +0x18  size_t _Res
```

**32-bit MSVC (x86):**
```
Pointers are 4 bytes instead of 8, so offsets are halved/adjusted
std::unordered_map: 0x00, 0x04, 0x08, 0x0C, 0x10
std::string: 0x00, 0x08, 0x0C
```

### Custom Configuration File

Create `roblox_config.json` (see `roblox_config_example.json` for template):

```json
{
  "version": "64-bit-custom-v2024",
  "is64Bit": true,
  "unordered_map": {
    "offsetBucketCount": "0x00",
    "offsetHead": "0x08",
    "offsetBucketArray": "0x10",
    "offsetSize": "0x18",
    "offsetMaxLoadFactor": "0x20"
  },
  "std_string": {
    "offsetPtr": "0x00",
    "offsetSize": "0x10",
    "offsetRes": "0x18",
    "ssoThreshold": 15
  },
  "node_structure": {
    "offsetNext": "0x00",
    "offsetHashCode": "0x08",
    "offsetKey": "0x10",
    "offsetValue": "0x30"
  },
  "limits": {
    "maxFlags": 150000,
    "minElements": 100,
    "maxBuckets": 2000000
  }
}
```

---

## Architecture Changes

### Config Struct
```cpp
struct Config {
  bool is64Bit;
  std::string version;
  
  // Map offsets
  size_t offsetBucketCount, offsetHead, offsetBucketArray, ...;
  
  // String offsets
  size_t offsetStringPtr, offsetStringSize, offsetStringRes;
  
  // Node offsets
  size_t offsetNodeNext, offsetNodeHashCode, offsetNodeKey, offsetNodeValue;
  
  // Process patterns
  std::vector<std::wstring> processNames;
  std::vector<std::string> flagPrefixes;
  
  // Static factory methods
  static Config GetDefaultFor(const std::string& version);
  static Config GetDefaultFor32Bit();
};
```

### Function Signatures Updated

All core functions now accept `const Config&`:

```cpp
// Before (v2.1)
std::vector<MapCandidate> FindMapCandidates(const MemoryReader& mem, 
                                             const std::vector<Section>& sections);

// After (v3.0)
std::vector<MapCandidate> FindMapCandidates(const MemoryReader& mem, 
                                             const std::vector<Section>& sections,
                                             const Config& cfg);
```

Functions updated:
- `FindMapCandidates()` - uses configurable offsets for map detection
- `TraverseMap()` - reads nodes with configurable offsets
- `ScanFlagStrings()` - uses configurable flag prefixes
- `FindGetterFunction()` - unchanged (x64-specific)
- `FindGlobalMapPointers()` - uses configurable pointer reading
- `MemoryReader` - accepts Config pointer for dynamic string reading

---

## How to Use

### 1. **Compile**
```bash
cl /O2 /MT /Fe:RobloxFlagDumper.exe RobloxFlagDumper.cpp /link psapi.lib version.lib
```

### 2. **Run**
```bash
RobloxFlagDumper.exe
```
Run as Administrator. Must have Roblox process running.

### 3. **Output**
```
RobloxFlags.hpp  - Dumped flags and offsets
dumper.log       - Detailed execution log
```

### 4. (Optional) **Configure**
- Copy `roblox_config_example.json` → `roblox_config.json`
- Adjust offsets if you're using a different compiler or Roblox version
- Program will detect and use it automatically

---

## Detection Flow

```
1. Find Roblox process (RobloxPlayerBeta.exe or RobloxPlayer.exe)
2. Detect 32-bit vs 64-bit architecture
3. Read version info from process EXE
4. Load Config (custom JSON or defaults)
5. Create MemoryReader with Config
6. Parse PE sections
7. Scan for flag strings with Config.flagPrefixes
8. Find map candidates using Config offsets
9. Vote for FFlagList and FlagToValue pointers
10. Traverse map using Config node offsets
11. Write RobloxFlags.hpp
```

---

## Extensibility

### Adding Support for a New Roblox Version

In the `Config::GetDefaultFor()` method, add version-specific adjustments:

```cpp
static Config GetDefaultFor(const std::string& version)
{
    Config cfg = /* default x64 config */;
    
    if (version.find("version-2024") != std::string::npos) {
        cfg.offsetNodeValue = 0x31;  // Hypothetical new offset
        cfg.ssoThreshold = 16;
    }
    
    return cfg;
}
```

### Adding Support for a Different Compiler

Create a new static factory method:

```cpp
static Config GetDefaultForGCC64Bit()
{
    Config cfg;
    cfg.is64Bit = true;
    cfg.offsetBucketCount = 0x00;  // GCC layout may differ
    // ... adjust all offsets
    return cfg;
}
```

### Loading from JSON

The `LoadOrCreateConfig()` function can be extended to parse JSON:

```cpp
Config LoadOrCreateConfig(const std::string& version, bool is64Bit)
{
    std::string cfgPath = "roblox_config.json";
    if (fs::exists(cfgPath)) {
        // TODO: Implement JSON parsing (e.g., using nlohmann/json)
        // For now, uses defaults as fallback
    }
    return is64Bit ? Config::GetDefaultFor(version) 
                   : Config::GetDefaultFor32Bit();
}
```

---

## Features (from v2.1, still present)

- **PE section-aware scanning** — searches `.rdata` for strings, `.text` for code, `.data` for maps
- **Multi-map voting** — detects FFlagList and FlagToValue separately
- **SSO-aware string reader** — handles both inline and heap-allocated strings
- **Prime-bucket heuristic** — MSVC uses prime bucket counts
- **Value type detection** — bool / int64 / string based on prefix
- **Roblox version in header** — reads FileVersionInfo
- **Single file, no dependencies** — only Windows API
- **Detailed logging** — console output with colors + `dumper.log`

---

## Example Output

```
=== Roblox Fast Flag Dumper v3.0 (Dynamic, Version-Aware) ===

[00:30:22.451] [INFO] Finding Roblox process
[00:30:22.452] [INFO] Found: RobloxPlayerBeta.exe
[00:30:22.452] [INFO] PID: 12345
[00:30:22.453] [INFO] Process opened successfully.

-- Detecting process architecture
[00:30:22.454] [INFO] Process type: 64-bit

-- Loading configuration
[00:30:22.454] [INFO] Configuration loaded for: version-663-0-12345

-- Reading module info
[00:30:22.455] [INFO] Base address : 0x00007FF600000000
[00:30:22.456] [INFO] Image size   : 0x2B400000 (684 MB)

-- Parsing PE sections
[00:30:22.457] [INFO] Sections found: 12
    .text       VA=0x00007FF600001000  size=0x19A00000  chars=0x60000020
    .rdata      VA=0x00007FF619B01000  size=0x0B700000  chars=0x40000040
    ...

-- Scanning for FFlag strings
[00:30:23.123] [INFO] FFlag strings found: 2847
    0x00007FF619C00100  "FFlagDebugDisplayFPS"
    0x00007FF619C00200  "FIntRenderingQuality"
    ...

-- Searching for unordered_map candidates
[00:30:24.567] [INFO] Candidates found: 47
    [0] addr=0x7FF6A3B4C200  elems= 13390  buckets=13399  score=65.0
    [1] addr=0x7FF6A3B4C2E0  elems=  2847  buckets= 2851  score=60.0
    ...

-- Voting for FFlagList and FlagToValue pointers
[00:30:24.890] [INFO] FFlagList  ptr : 0x7FF6A3127A00  ->  map @ 0x7FF6A3B4C200
[00:30:24.891] [INFO] FlagToValue ptr: 0x7FF6A3127A08  ->  map @ 0x7FF6A3B4C2E0

-- Traversing FFlagList map
[00:30:24.950] [INFO] Raw nodes read: 13390

[progress] 100% (13390/13390 flags)
[progress] done – flag strings

[00:30:25.123] [INFO] Type breakdown  →  bool=10234  int=2005  string=1151  unknown=0

    Sample flags:
    FFlagDebugDisplayFPS = true  @ 0x7FF6AB12C340
    FIntRenderingQuality = 2  @ 0x7FF6AB12D080
    ...

-- Writing output files
[00:30:25.200] [INFO] Roblox version: version-663-0-12345
[00:30:25.300] [INFO] RobloxFlags.hpp written  (13390 flags)
[00:30:25.300] [INFO] dumper.log      written

[00:30:25.301] [INFO] All done. Exiting.
```

---

## Troubleshooting

### "No FFlag strings found"
- Roblox may have changed string encoding or layout
- Check if offsets in `Config::flagPrefixes` need adjustment
- Create custom `roblox_config.json` with your version's patterns

### "No map candidates found"
- Memory layout has changed significantly
- Update `Config` offsets for the new Roblox version
- Run with `/DEBUG` flag (if implemented) for verbose offset testing

### "Map traversal failed"
- Node structure offsets (`offsetNodeNext`, `offsetNodeKey`, `offsetNodeValue`) are wrong
- Verify with a debugger (x64dbg, WinDbg)
- Update `roblox_config.json` with corrected values

### "32-bit process detected but code fails"
- x64 instruction patterns are hardcoded in some places
- 32-bit machine code uses different instructions
- Extend `IsLeaRdxRip()`, `IsMovRcxRip()` patterns for x86 if needed

---

## Files

- `RobloxFlagDumper.cpp` - Main executable code
- `roblox_config_example.json` - Configuration template (documentation only)
- `RobloxFlags.hpp` - Output: offsets and node addresses
- `dumper.log` - Output: execution log (plain text)

---

## License & Disclaimer

Educational purposes only. Respect Roblox Terms of Service. No warranty.

---

**Version:** 3.0 (Dynamic, Version-Aware)  
**Last Updated:** 2026-06-14
