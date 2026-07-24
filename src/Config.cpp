#include "Config.h"
#include "Logger.h"

Config Config::GetDefaultFor(const std::string& version)
{
    Config cfg;
    // Default MSVC x64 layout (works for most versions)
    cfg.is64Bit = true;
    cfg.version = version;
    
    // These offsets are consistent across MSVC versions
    cfg.offsetBucketCount   = 0x00;
    cfg.offsetHead          = 0x08;
    cfg.offsetBucketArray   = 0x10;
    cfg.offsetSize          = 0x18;
    cfg.offsetMaxLoadFactor = 0x20;
    
    cfg.offsetStringPtr     = 0x00;
    cfg.offsetStringSize    = 0x10;
    cfg.offsetStringRes     = 0x18;
    cfg.ssoThreshold        = 15;
    cfg.sizeofMsvcString    = 0x20;   // 8 (ptr) + 8 (size) + 8 (res), aligned
    
    cfg.offsetNodeNext      = 0x00;
    cfg.offsetNodeHashCode  = 0x08;
    cfg.offsetNodeKey       = 0x10;
    cfg.offsetNodeValue     = 0x30;
    
    return cfg;
}

Config Config::GetDefaultFor32Bit()
{
    Config cfg;
    cfg.is64Bit = false;
    cfg.version = "32-bit";
    
    // 32-bit MSVC offsets (pointers are 4 bytes)
    cfg.offsetBucketCount   = 0x00;
    cfg.offsetHead          = 0x04;
    cfg.offsetBucketArray   = 0x08;
    cfg.offsetSize          = 0x0C;
    cfg.offsetMaxLoadFactor = 0x10;
    
    cfg.offsetStringPtr     = 0x00;
    cfg.offsetStringSize    = 0x08;
    cfg.offsetStringRes     = 0x0C;
    cfg.ssoThreshold        = 15;
    cfg.sizeofMsvcString    = 0x10;   // 4 (ptr) + 4 (size) + 4 (res), aligned to 4
    
    cfg.offsetNodeNext      = 0x00;
    cfg.offsetNodeHashCode  = 0x04;
    cfg.offsetNodeKey       = 0x08;
    cfg.offsetNodeValue     = 0x1C;
    
    return cfg;
}

Config LoadOrCreateConfig(const std::string& version, bool is64Bit)
{
    Config cfg = is64Bit ? Config::GetDefaultFor(version)
                          : Config::GetDefaultFor32Bit();

    Log::Info("Using default offsets for " + std::string(is64Bit ? "64-bit" : "32-bit") + " process");
    return cfg;
}