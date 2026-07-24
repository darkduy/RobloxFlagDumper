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
    size_t sizeofMsvcString     = 0x20;   // sizeof(std::string) on this ABI (32 on x64, 24 on x86 — set in Get...() below)
    
    // Node structure offsets
    size_t offsetNodeNext       = 0x00;   // _Next
    size_t offsetNodeHashCode   = 0x08;   // _Hash_code
    size_t offsetNodeKey        = 0x10;   // _Kval (key is std::string)
    size_t offsetNodeValue      = 0x30;   // _Myval — starting guess only;
                                           // Roblox's internal FVariable layout can shift
                                           // between builds, so TraverseMap() re-detects
                                           // this at runtime via DetectNodeValueOffset()
                                           // and only falls back to this value if detection fails.
    
    // Process search patterns
    std::vector<std::wstring> processNames = { L"RobloxPlayerBeta.exe", L"RobloxPlayer.exe" };
    
    // Constants
    size_t maxFlags     = 150'000;   // giới hạn an toàn khi duyệt map (chống vòng lặp/dữ liệu hỏng)
    size_t maxStrLen    = 128;       // độ dài tối đa khi đọc std::string từ tiến trình

    // Runtime node-value offset detection (see DetectNodeValueOffset in MapTraversal.cpp)
    size_t valueScanRange = 0x20;    // số byte quét sau khi key kết thúc để tìm value hợp lệ
    
    // Version-specific defaults
    static Config GetDefaultFor(const std::string& version);
    static Config GetDefaultFor32Bit();
};

// Load configuration from JSON file or create defaults
Config LoadOrCreateConfig(const std::string& version, bool is64Bit);