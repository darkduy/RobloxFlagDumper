#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ─────────────────────────────── CONFIGURATION ───────────────────────────
// Offsets and settings that vary by Roblox version
struct Config
{
    // Process information
    bool is64Bit = true;
    std::string version = "unknown";
    
    // unordered_map<string, T> MSVC offsets
    size_t offsetBucketCount    = 0x00;   // _Bucket_count
    size_t offsetHead           = 0x08;   // _Head (first node)
    size_t offsetBucketArray    = 0x10;   // _Buckets
    size_t offsetSize           = 0x18;   // _Size (element count)
    size_t offsetMaxLoadFactor  = 0x20;   // _Max_load_factor
    
    // std::string MSVC offsets
    size_t offsetStringPtr      = 0x00;   // _Ptr or inline buffer
    size_t offsetStringSize     = 0x10;   // _Size
    size_t offsetStringRes      = 0x18;   // _Res (capacity)
    size_t ssoThreshold         = 15;     // Small String Optimization threshold
    
    // Node structure offsets
    size_t offsetNodeNext       = 0x00;   // _Next
    size_t offsetNodeHashCode   = 0x08;   // _Hash_code
    size_t offsetNodeKey        = 0x10;   // _Kval (key is std::string)
    size_t offsetNodeValue      = 0x30;   // _Myval (value starts here)
    
    // Process search patterns
    std::vector<std::wstring> processNames = { L"RobloxPlayerBeta.exe", L"RobloxPlayer.exe" };
    
    // Flag name prefixes to scan for
    std::vector<std::string> flagPrefixes = { 
        "FFlag", "DFFlag", "SFFlag", "FInt", "DFInt", "FString", "DFString" 
    };
    
    // Constants
    size_t maxFlags     = 150'000;
    size_t minElements  = 100;
    size_t maxBuckets   = 2'000'000;
    size_t maxStrLen    = 128;
    size_t readChunk    = 0x1000;
    uint32_t loadFactorIEEE = 0x3F800000u;  // 1.0f in IEEE 754
    
    // Version-specific defaults
    static Config GetDefaultFor(const std::string& version);
    static Config GetDefaultFor32Bit();
};

// Load configuration from JSON file or create defaults
Config LoadOrCreateConfig(const std::string& version, bool is64Bit);
