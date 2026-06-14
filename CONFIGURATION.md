# Configuration Guide - RobloxFlagDumper v3.0

This guide explains how to configure RobloxFlagDumper for different Roblox versions and architectures.

## Quick Start

**No configuration needed!** The tool includes smart defaults that work for most Roblox versions.

Just compile and run:
```bash
cl /O2 /MT /Fe:RobloxFlagDumper.exe RobloxFlagDumper.cpp /link psapi.lib version.lib
RobloxFlagDumper.exe
```

## When Do You Need a Config File?

- ❌ **Don't need:** Using current Roblox version, it works fine
- ✅ **Need:** Roblox changed memory layout, tool can't find flags
- ✅ **Need:** Supporting 32-bit process with different layout
- ✅ **Need:** Roblox using non-standard compiler or optimizations

## Creating a Config File

### Step 1: Copy the Template
```bash
copy roblox_config_example.json roblox_config.json
```

### Step 2: Edit Offsets (if needed)

The most important offsets are for `std::unordered_map`:

```json
{
  "unordered_map": {
    "offsetBucketCount": "0x00",    // Map.buckets array size
    "offsetHead": "0x08",           // First node pointer
    "offsetBucketArray": "0x10",    // Buckets array pointer
    "offsetSize": "0x18",           // Number of elements
    "offsetMaxLoadFactor": "0x20"   // Should be 0x3F800000 (1.0f)
  }
}
```

**How to find these:**
1. Use debugger (WinDbg, x64dbg)
2. Set breakpoint in Roblox code that accesses the flag map
3. Inspect the map structure in memory
4. Note the offsets of each field

### Step 3: Test
```bash
RobloxFlagDumper.exe
# If it works, great! If not, adjust offsets and try again.
```

## Understanding the Config Structure

### Map Structure (std::unordered_map<std::string, T>)
```
Address+0x00: size_t _Bucket_count    (64-bit) or _Bucket_count (32-bit)
Address+0x08: _Node* _Head            (64-bit only: pointer size = 8)
Address+0x04: _Node* _Head            (32-bit: pointer size = 4)
Address+0x10: _Bucket* _Buckets       (64-bit)
Address+0x08: _Bucket* _Buckets       (32-bit)
Address+0x18: size_t _Size            (64-bit)
Address+0x0C: size_t _Size            (32-bit)
Address+0x20: float _Max_load_factor  (64-bit)
Address+0x10: float _Max_load_factor  (32-bit)
```

### String Structure (std::string, MSVC)
```
Address+0x00: char* _Ptr (or inline data for SSO)
Address+0x10: size_t _Size
Address+0x18: size_t _Res (capacity)

If _Res < 16: string is stored inline at +0x00
If _Res >= 16: string data at _Ptr address
```

### Node Structure (unordered_map node)
```
Address+0x00: _Node* _Next             (next node in list)
Address+0x08: size_t _Hash_code        (cached hash)
Address+0x10: std::string _Kval        (key)
Address+0x30: Mapped _Myval            (value)
```

## Version-Specific Configurations

### Roblox 32-bit
If running 32-bit Roblox, use 32-bit offsets in your config:
```json
{
  "is64Bit": false,
  "unordered_map": {
    "offsetBucketCount": "0x00",
    "offsetHead": "0x04",
    "offsetBucketArray": "0x08",
    "offsetSize": "0x0C",
    "offsetMaxLoadFactor": "0x10"
  }
}
```

### Roblox with Different Compiler
Different compilers might use different struct layouts. Measure with debugger and create a config:

```json
{
  "version": "custom-gcc-build",
  "is64Bit": true,
  "unordered_map": {
    "offsetBucketCount": "0x00",
    "offsetHead": "0x08",
    "offsetBucketArray": "0x10",
    "offsetSize": "0x20",    // Different offset!
    "offsetMaxLoadFactor": "0x28"
  }
}
```

## Debugging Configuration Issues

### Issue: "No map candidates found"

**Likely cause:** `offsetSize` is wrong - tool can't identify valid maps.

**Debug steps:**
1. Open Roblox in debugger (x64dbg)
2. Find address of flag list (search for magic bytes or known flag name)
3. Measure distance to element count field
4. Update `offsetSize` in config
5. Run RobloxFlagDumper again

### Issue: "Map traversal failed"

**Likely cause:** Node offsets wrong (`offsetNodeNext`, `offsetNodeKey`, etc.)

**Debug steps:**
1. Note the first flag's node address from output
2. Open debugger and navigate to that address
3. Check:
   - `offset+0x00`: Should be next node pointer (or 0 if last)
   - `offset+0x10`: Should be a valid string address or small value
4. Adjust `offsetNodeKey`, `offsetNodeValue` accordingly

### Issue: "FFlag strings found: 0"

**Likely cause:** Flag name prefixes are wrong or string encoding changed.

**Debug steps:**
1. Search for a known flag name (e.g., "FFlagDebugDisplayFPS")
2. If found, flag prefixes are okay
3. If not found, Roblox may have changed encoding
4. Adjust `flagPrefixes` in config to match what you find

## Creating Multiple Configs

You can have different configs for different scenarios:

```bash
roblox_config.json              # Default
roblox_config.32bit.json        # For 32-bit Roblox
roblox_config.experimental.json # For Roblox Beta
```

Then specify which to use (if you modify `LoadOrCreateConfig()`):
```cpp
Config LoadOrCreateConfig(const std::string& cfgFile) {
    if (fs::exists(cfgFile)) {
        // Load from cfgFile
    }
    return Config::GetDefaultFor(version);
}
```

## Configuration Best Practices

### 1. Keep Defaults as Fallback
Always ensure defaults work for current Roblox version. Configs should only be for edge cases.

### 2. Document Your Changes
```json
{
  "version": "roblox-2024-q2-custom",
  "comment": "Custom config for Roblox 2024 Q2 with PGO optimizations",
  "is64Bit": true,
  "offsetNodeValue": "0x31",
  "comment_reason": "PGO changed node layout by 1 byte"
}
```

### 3. Test Before Deploying
Always test a new config:
```bash
RobloxFlagDumper.exe
# Check if output looks reasonable
# Compare flag count with previous runs
```

### 4. Version Your Configs
```
roblox_config_2024-03.json
roblox_config_2024-06.json
roblox_config_2024-09.json
```

## Advanced: Extending Configuration

### Adding New Offset for Non-MSVC Builds

If you encounter non-MSVC builds, extend Config:

```cpp
struct Config {
  // ... existing fields ...
  
  std::string compiler;  // "msvc", "gcc", "clang"
  
  static Config GetDefaultForGCC64Bit() {
    Config cfg;
    cfg.compiler = "gcc";
    cfg.is64Bit = true;
    cfg.offsetBucketCount = 0x00;  // GCC layout
    // ... adjust other offsets ...
    return cfg;
  }
};
```

### Custom Flag Patterns

Add your own flag prefixes to detect custom flags:

```json
{
  "flag_patterns": {
    "prefixes": [
      "FFlag", "DFFlag", "SFFlag",
      "FInt", "DFInt",
      "FString", "DFString",
      "XFlag"  // Custom prefix
    ]
  }
}
```

### Dynamic Offset Discovery (Future)

You could implement automatic offset detection:

```cpp
// Pseudocode
std::vector<size_t> FindMapCandidateOffsets(const MemoryReader& mem) {
    std::vector<size_t> candidates;
    for (size_t off = 0; off < 0x100; off += 8) {
        // Test if (mapAddr + off) looks like valid element count
        // Test if (mapAddr + off) looks like valid pointer
    }
    return candidates;
}
```

## Common Offset Values

### MSVC x64 (Default)
```
Map:    0x00, 0x08, 0x10, 0x18, 0x20
String: 0x00, 0x10, 0x18
Node:   0x00, 0x08, 0x10, 0x30
```

### MSVC x86
```
Map:    0x00, 0x04, 0x08, 0x0C, 0x10
String: 0x00, 0x08, 0x0C
Node:   0x00, 0x04, 0x08, 0x1C
```

### GCC/Clang x64 (may vary)
```
Map:    similar to MSVC
String: may differ (different SSO implementation)
Node:   similar to MSVC
```

## Troubleshooting Checklist

- [ ] Is Roblox running?
- [ ] Running as Administrator?
- [ ] Process architecture matches config (64-bit cfg for 64-bit Roblox)?
- [ ] `offsetSize` value reasonable (< 100)?
- [ ] `offsetBucketCount` exists and is > 0?
- [ ] Process is RobloxPlayer*.exe, not launcher?
- [ ] No other FFlag dumpers running (port conflicts)?

## Support

If offsets don't work:

1. **Compare with working version** - What changed?
2. **Use debugger** - Step through one flag access
3. **Measure exactly** - Don't guess, measure with x64dbg
4. **Document findings** - Share with community

## References

- MSVC STL implementation: `/Program Files/Microsoft Visual Studio/...`
- x64dbg: https://x64dbg.com/ (free debugger)
- WinDbg: Debugging Tools for Windows
- Roblox reverse engineering: Community forums

---

**Version:** 3.0  
**Last Updated:** 2026-06-14
