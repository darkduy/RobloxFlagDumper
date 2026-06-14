# 📚 Documentation Index - RobloxFlagDumper v3.0

## 🎯 Start Here

- **[QUICKSTART.md](QUICKSTART.md)** ⭐ - 2-minute overview of what changed (START HERE!)
- **[README_DYNAMIC.md](README_DYNAMIC.md)** - Comprehensive user guide with all features

## 📖 Detailed Guides

- **[CONFIGURATION.md](CONFIGURATION.md)** - How to configure offsets for different Roblox versions
- **[CHANGES_v3.0.md](CHANGES_v3.0.md)** - Technical changelog with code examples

## 📄 Files in This Repository

### Source Code
- **RobloxFlagDumper.cpp** - Main program (updated to v3.0 with Config system)

### Configuration
- **roblox_config_example.json** - Template for custom configurations
- **roblox_config.json** - Your custom config (optional, auto-created)

### Documentation
- **README_DYNAMIC.md** - Full feature documentation
- **QUICKSTART.md** - Quick overview (5 min read)
- **CONFIGURATION.md** - How-to guide for custom configs
- **CHANGES_v3.0.md** - Detailed technical changes
- **Readme.md** - Original readme (v2.1 documentation)

## 🚀 Quick Commands

### Compile
```bash
cl /O2 /MT /Fe:RobloxFlagDumper.exe RobloxFlagDumper.cpp /link psapi.lib version.lib
```

### Run
```bash
RobloxFlagDumper.exe     # Uses smart defaults
```

### With Custom Config
```bash
copy roblox_config_example.json roblox_config.json
# Edit roblox_config.json as needed
RobloxFlagDumper.exe     # Auto-loads config.json
```

## ❓ Help Choosing a Document

### "I just want to use it" → [QUICKSTART.md](QUICKSTART.md)
- What changed from v2.1?
- How do I run it?
- What's the benefit?

### "I want full details" → [README_DYNAMIC.md](README_DYNAMIC.md)
- Complete feature documentation
- Architecture overview
- Extensibility guide
- Example output

### "I need custom offsets" → [CONFIGURATION.md](CONFIGURATION.md)
- How to create config files
- How to find correct offsets
- Version-specific configurations
- Debugging tips

### "I'm a developer" → [CHANGES_v3.0.md](CHANGES_v3.0.md)
- Line-by-line code changes
- New functions and structures
- Integration points
- Backwards compatibility info

### "What's in the code?" → [RobloxFlagDumper.cpp](RobloxFlagDumper.cpp)
- Line ~100-180: Config struct definition
- Line ~165-180: LoadOrCreateConfig() function
- Line ~200-250: Enhanced MemoryReader with Config
- Line ~510-570: Dynamic FindMapCandidates()
- Line ~930-1100: Refactored main() function

## 🎨 What Changed in v3.0?

### The Big Picture
| Aspect | Before | After |
|--------|--------|-------|
| Offsets | Hardcoded | Configurable |
| 32-bit | Not supported | Supported |
| Version aware | No | Yes (auto-detect) |
| Config files | No | Yes (optional JSON) |
| Extensible | Hard | Easy |

### Key Improvements
1. **Config Struct** - All memory layout info in one place
2. **Version Detection** - Automatically detects Roblox version & architecture
3. **32-bit Support** - Works on both x86 and x64
4. **JSON Configuration** - Customize without recompiling
5. **Dynamic Offsets** - All offsets are data, not code

## 📊 Feature Checklist

- ✅ Zero hardcoded offsets
- ✅ Automatic process detection
- ✅ Architecture detection (32/64-bit)
- ✅ Version detection from process
- ✅ Configurable via JSON file (optional)
- ✅ Multiple executable names supported
- ✅ SSO-aware string reading
- ✅ Prime-bucket heuristic
- ✅ Value type detection (bool/int/string)
- ✅ Roblox version in output header
- ✅ Single-file, no dependencies
- ✅ Detailed logging with timestamps
- ✅ 100% backwards compatible

## 🔧 Troubleshooting

### "It doesn't find any flags"
→ See [CONFIGURATION.md - Debugging](CONFIGURATION.md#debugging-configuration-issues)

### "Which config should I use?"
→ See [QUICKSTART.md - No Config Needed](QUICKSTART.md#no-config-file-needed-just-works)

### "How do I update for a new Roblox version?"
→ See [CONFIGURATION.md - Version-Specific](CONFIGURATION.md#version-specific-configurations)

### "Can I modify the code?"
→ See [CHANGES_v3.0.md - Extensibility](CHANGES_v3.0.md#extensibility)

## 📞 Common Questions

**Q: Do I need a config file?**
A: No! Uses smart defaults. Only need it if Roblox changes layout.

**Q: Is this slower than v2.1?**
A: No, same speed. All changes are compile-time or single dereferences.

**Q: Can I use old scripts?**
A: Yes! v3.0 is fully backwards compatible. No breaking changes.

**Q: How do I support a new Roblox version?**
A: Usually automatic! If not, either:
   1. Create a `roblox_config.json` with new offsets (no recompile)
   2. Add version-specific code in `Config::GetDefaultFor()`

**Q: Does it work on 32-bit Roblox?**
A: Yes! Auto-detects and uses 32-bit offsets.

**Q: Can I use this with non-MSVC builds?**
A: Currently MSVC only, but easy to extend. See [CONFIGURATION.md - Extending](CONFIGURATION.md#advanced-extending-configuration)

## 🎓 Learning Resources

### For End Users
1. [QUICKSTART.md](QUICKSTART.md) - Learn what changed
2. [README_DYNAMIC.md](README_DYNAMIC.md) - Learn how to use it
3. [CONFIGURATION.md](CONFIGURATION.md) - Learn to configure it

### For Developers
1. [CHANGES_v3.0.md](CHANGES_v3.0.md) - Code changes
2. [README_DYNAMIC.md](README_DYNAMIC.md#extensibility) - How to extend
3. [RobloxFlagDumper.cpp](RobloxFlagDumper.cpp) - Read the code

### For Debuggers
1. [CONFIGURATION.md#debugging-configuration-issues](CONFIGURATION.md#debugging-configuration-issues)
2. [README_DYNAMIC.md#troubleshooting](README_DYNAMIC.md#troubleshooting)

## 📈 Version History

- **v3.0** (2026-06-14) - Dynamic configuration system, 32-bit support, version detection
- **v2.1** - Original hardcoded version
- **v1.0** - Initial release

## 📝 Document Quick Reference

```
QUICKSTART.md
├─ What changed
├─ Before/after comparison
├─ File structure overview
└─ When do you need config

README_DYNAMIC.md
├─ Full feature list
├─ Architecture overview
├─ Configuration structure
├─ Extensibility guide
└─ Example output

CONFIGURATION.md
├─ Quick start (no config needed)
├─ Creating custom configs
├─ Understanding structures
├─ Version-specific configs
├─ Debugging tips
└─ Common offset values

CHANGES_v3.0.md
├─ Technical changes
├─ Code examples (before/after)
├─ Modified functions list
├─ New files created
├─ Backwards compatibility
└─ Testing recommendations

RobloxFlagDumper.cpp
├─ Main source code
├─ Config struct (~100-180)
├─ MemoryReader (~200-250)
├─ Map finding (~510-570)
├─ Map traversal (~600-630)
└─ main() (~930-1100+)
```

## 🎯 Next Steps

1. **Read**: [QUICKSTART.md](QUICKSTART.md) (5 minutes)
2. **Compile**: Follow instructions in README or QUICKSTART
3. **Run**: `RobloxFlagDumper.exe` (no config needed!)
4. **Output**: Check `RobloxFlags.hpp` for dumped flags
5. **Configure** (optional): If it doesn't work, see [CONFIGURATION.md](CONFIGURATION.md)

---

**Last Updated:** 2026-06-14  
**Version:** 3.0 (Dynamic Edition)  
**Status:** ✅ Production Ready
