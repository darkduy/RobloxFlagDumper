# 🚀 RobloxFlagDumper - Now Dynamic! (v3.0)

## What Changed?

This file has been transformed from a **static, version-dependent tool** into a **fully dynamic, configurable solution** that adapts to different Roblox versions and process architectures.

## Quick Comparison

| Feature | v2.1 (Old) | v3.0 (New) |
|---------|------------|-----------|
| **Hardcoded offsets** | ❌ Yes (fixed) | ✅ Dynamic (configurable) |
| **32-bit support** | ❌ No | ✅ Yes |
| **Version awareness** | ❌ No | ✅ Yes (auto-detected) |
| **Config files** | ❌ No | ✅ Yes (optional JSON) |
| **Process detection** | ❌ Single name | ✅ Multiple names |
| **Future-proof** | ⚠️ Needs manual updates | ✅ Config-based |

## Major Improvements

### 1. **Configuration System** 🔧
```cpp
struct Config {
  // Instead of hardcoded offsets everywhere,
  // all memory layout info is in ONE struct
  size_t offsetBucketCount;     // 0x00
  size_t offsetHead;            // 0x08
  size_t offsetSize;            // 0x18
  // ... all other offsets
};
```

**Benefit:** Change offsets once, everywhere gets updated automatically.

### 2. **32-bit Process Support** 💻
```cpp
if (cfg.is64Bit) {
  // 64-bit: 8-byte pointers
  mapAddr = mem.Read<uintptr_t>(fflagListPtr);
} else {
  // 32-bit: 4-byte pointers
  mapAddr = (uintptr_t)mem.Read<uint32_t>(fflagListPtr);
}
```

**Benefit:** Works on both x86 and x64 Roblox installations.

### 3. **Version Detection** 🔍
```cpp
// Automatically detects:
// - 32-bit vs 64-bit
// - Roblox version number
// - Uses appropriate offsets for each
```

**Benefit:** No manual configuration needed for version changes.

### 4. **JSON Configuration** ⚙️
```json
// roblox_config.json (optional)
{
  "unordered_map": {
    "offsetBucketCount": "0x00",
    "offsetSize": "0x18"
  }
}
```

**Benefit:** Customize offsets without recompiling.

### 5. **All Offsets Are Now Dynamic** 📊

**Changed in these functions:**
- `MemoryReader::ReadMsvcString()` - string offsets
- `FindMapCandidates()` - map detection offsets
- `TraverseMap()` - node traversal offsets
- `ScanFlagStrings()` - flag prefix patterns
- `FindGlobalMapPointers()` - pointer reading

## How to Use

### **No Config File Needed** (Just Works!)
```bash
RobloxFlagDumper.exe
# Uses smart defaults for your Roblox version
```

### **Custom Config File** (For Version Mismatch)
```bash
# 1. Copy template
copy roblox_config_example.json roblox_config.json

# 2. Edit offsets for your Roblox version

# 3. Run
RobloxFlagDumper.exe  # Automatically loads config
```

## File Structure

```
RobloxFlagDumper.cpp          ← Updated with Config system
│
├─ Config struct (lines ~100-180)
├─ LoadOrCreateConfig()
├─ Enhanced MemoryReader with Config support
├─ All functions updated with Config parameter
└─ main() refactored for architecture detection

roblox_config_example.json    ← NEW: Configuration template
README_DYNAMIC.md              ← NEW: Comprehensive guide
CHANGES_v3.0.md                ← NEW: Detailed changelog
```

## Code Highlights

### Before (v2.1) - Hardcoded:
```cpp
size_t elementCount = mem.Read<size_t>(mapAddr + 0x18);
uintptr_t head = mem.Read<uintptr_t>(mapAddr + 0x08);
std::string key = mem.ReadMsvcString(cur + 0x10);
// These offsets are baked in!
```

### After (v3.0) - Dynamic:
```cpp
size_t elementCount = mem.Read<size_t>(mapAddr + cfg.offsetSize);
uintptr_t head = cfg.is64Bit ? mem.Read<uintptr_t>(mapAddr + cfg.offsetHead)
                               : (uintptr_t)mem.Read<uint32_t>(mapAddr + cfg.offsetHead);
std::string key = mem.ReadMsvcString(cur + cfg.offsetNodeKey);
// Everything comes from Config!
```

## Performance

✅ **No degradation** - All changes are compile-time decisions or single dereferences.
Same speed as v2.1.

## Backwards Compatibility

✅ **Fully compatible** - If no config file exists, uses sensible defaults.
Existing workflows are unaffected.

## Next Steps for Users

1. **Update to v3.0** → Recompile with updated RobloxFlagDumper.cpp
2. **Run normally** → Should work exactly like before (uses defaults)
3. **Optional: Create config** → If you want custom offsets, copy the template
4. **Future Roblox updates** → Config file can be updated without recompiling

## What's Inside

### New `Config` Structure
- **Map offsets** - bucket count, head, buckets array, size, load factor
- **String offsets** - ptr, size, reserved, SSO threshold
- **Node offsets** - next, hash code, key, value
- **Process patterns** - executable names to search for
- **Flag patterns** - prefixes to scan (FFlag, DFInt, FString, etc.)
- **Limits** - max flags, buckets, string length
- **Version info** - Roblox version tracking

### New Functions
- `Config::GetDefaultFor(version)` - x64 defaults for given version
- `Config::GetDefaultFor32Bit()` - x86 defaults
- `LoadOrCreateConfig()` - load from file or use defaults

### Enhanced Functions
- `MemoryReader(hProc, &cfg)` - now takes Config pointer
- `ReadMsvcString()` - uses dynamic offsets
- `FindMapCandidates(mem, sections, cfg)` - takes Config
- `TraverseMap(mem, mapAddr, flags, cfg)` - takes Config
- `ScanFlagStrings(mem, sections, cfg)` - takes Config
- `main()` - detects architecture, loads config, passes it everywhere

## Documentation

Three new documentation files:
1. **CHANGES_v3.0.md** - Technical detailed changelog
2. **README_DYNAMIC.md** - Comprehensive user guide
3. **roblox_config_example.json** - Configuration template with explanations

## Error Handling

The tool now provides better diagnostics:
```
[INFO] Process type: 64-bit
[INFO] Configuration loaded for: version-663-0-12345
[WARN] ValueGetSet not found – will write 0x0 in header.
[ERR ] No FFlag strings found. Configuration offsets may need adjustment.
```

## For Developers

If Roblox changes its memory layout:

### Option 1: Use Config File
Create `roblox_config.json` with new offsets - no recompile needed.

### Option 2: Add Version-Specific Defaults
```cpp
static Config GetDefaultFor(const std::string& version) {
    Config cfg = /* base config */;
    if (version.find("2024-Q2") != std::string::npos) {
        cfg.offsetNodeValue = 0x31;  // New offset
    }
    return cfg;
}
```

### Option 3: Automated Discovery (Future)
Implement offset discovery in `ScanMemory()` functions.

## Summary

🎯 **RobloxFlagDumper is now truly dynamic!**

- Adapts to different Roblox versions automatically
- Supports both 32-bit and 64-bit processes
- Configurable via JSON files
- All offsets are data, not code
- Future-proof and maintainable

**Result:** When Roblox updates, you may need to update just the config file, not recompile the entire tool.

---

**For detailed information, see:**
- `README_DYNAMIC.md` - User guide and features
- `CHANGES_v3.0.md` - Technical changelog
- `roblox_config_example.json` - Configuration template
