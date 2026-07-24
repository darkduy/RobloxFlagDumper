#include "MapTraversal.h"
#include "MemoryReader.h"
#include "Config.h"
#include "Logger.h"
#include <set>
#include <cstdio>

// Đọc giá trị flag tại một địa chỉ TUYỆT ĐỐI (không phải offset tương đối với node).
// Kiểu dữ liệu được đoán từ tiền tố tên flag (quy ước đặt tên của Roblox FastFlag).
FlagValue ReadFlagValue(const MemoryReader& mem, uintptr_t valueAddr, const std::string& name)
{
    FlagValue fv;
    bool isInt    = (name.rfind("FInt",   0) == 0 || name.rfind("DFInt",  0) == 0);
    bool isString = (name.rfind("FString",0) == 0 || name.rfind("DFString",0)== 0);
    // FFlag/DFFlag (bool) là mặc định khi không khớp isInt/isString

    if (isInt) {
        fv.type   = FlagType::Int;
        fv.intVal = mem.Read<int64_t>(valueAddr);
    } else if (isString) {
        fv.type   = FlagType::String;
        fv.strVal = mem.ReadMsvcString(valueAddr);
    } else {
        uint8_t b  = mem.Read<uint8_t>(valueAddr);
        fv.type    = FlagType::Bool;
        fv.boolVal = (b != 0);
    }
    return fv;
}

// Đánh giá độ hợp lý của một giá trị đọc được, dựa trên kiểu suy ra từ tên flag.
// Dùng để chấm điểm các offset ứng viên khi dò offsetNodeValue tại runtime.
static bool IsPlausibleValue(const FlagValue& fv, uint8_t rawByte, int64_t rawInt)
{
    switch (fv.type) {
        case FlagType::Bool:
            // FFlag luôn là bool 1 byte: giá trị thô phải là 0 hoặc 1.
            return rawByte == 0 || rawByte == 1;
        case FlagType::Int:
            // Hầu hết FInt của Roblox nằm trong khoảng vừa phải; loại các giá trị
            // trông giống con trỏ hoặc rác bộ nhớ (quá lớn / âm bất thường).
            return rawInt >= -1'000'000 && rawInt <= 100'000'000;
        case FlagType::String:
            // ReadMsvcString() đã tự lọc non-ASCII và trả rỗng nếu không hợp lệ;
            // một FString hợp lệ nên có nội dung (chuỗi rỗng cũng có thể đúng
            // nhưng ít cho tín hiệu để chấm điểm, nên không tính là "plausible").
            return !fv.strVal.empty();
        default:
            return false;
    }
}

// In offset dạng hex ngắn gọn cho log, ví dụ ToHex(0x30) -> "30"
static std::string ToHex(size_t v)
{
    char buf[20];
    snprintf(buf, sizeof(buf), "%zX", v);
    return std::string(buf);
}

// Dò offsetNodeValue tại runtime thay vì tin tưởng tuyệt đối vào giá trị hardcode
// trong Config. Layout nội bộ của Roblox (struct chứa FVariable/FFlag) có thể lệch
// vài byte giữa các bản build, nên ta quét một dải offset ngay sau khi std::string
// key kết thúc và chọn offset đầu tiên cho ra giá trị "hợp lý" trên nhiều node mẫu.
//
// Trả về offset dò được, hoặc cfg.offsetNodeValue (giá trị mặc định) nếu dò thất bại.
static size_t DetectNodeValueOffset(
    const MemoryReader& mem,
    const std::vector<uintptr_t>& sampleNodes,
    const std::vector<std::string>& sampleKeys,
    const Config& cfg)
{
    size_t scanStart = cfg.offsetNodeKey + cfg.sizeofMsvcString;   // ngay sau khi key kết thúc
    size_t scanEnd   = scanStart + cfg.valueScanRange;

    // Căn lề 8-byte (x64) hoặc 4-byte (x86) vì MSVC align struct member theo word size.
    size_t alignment = cfg.is64Bit ? 8 : 4;

    size_t bestOffset = cfg.offsetNodeValue;   // fallback
    size_t bestScore   = 0;

    for (size_t off = scanStart; off <= scanEnd; off += alignment) {
        size_t score = 0;
        for (size_t i = 0; i < sampleNodes.size(); ++i) {
            uintptr_t valueAddr = sampleNodes[i] + off;
            const std::string& key = sampleKeys[i];

            FlagValue fv = ReadFlagValue(mem, valueAddr, key);
            uint8_t  rawByte = mem.Read<uint8_t>(valueAddr);
            int64_t  rawInt  = mem.Read<int64_t>(valueAddr);

            if (IsPlausibleValue(fv, rawByte, rawInt)) score++;
        }
        if (score > bestScore) {
            bestScore  = score;
            bestOffset = off;
        }
    }

    // Yêu cầu tối thiểu quá bán số mẫu khớp mới coi là dò thành công;
    // nếu không, offset mặc định trong Config an toàn hơn là một offset "trúng ngẫu nhiên".
    if (bestScore * 2 < sampleNodes.size()) {
        Log::Warn("DetectNodeValueOffset: khong du tin cay (" + std::to_string(bestScore) +
                   "/" + std::to_string(sampleNodes.size()) + "), dung offset mac dinh 0x" +
                   ToHex(cfg.offsetNodeValue));
        return cfg.offsetNodeValue;
    }

    return bestOffset;
}

bool TraverseMap(
    const MemoryReader& mem,
    uintptr_t mapAddr,
    std::vector<FlagEntry>& out,
    const Config& cfg)
{
    size_t elementCount = mem.Read<size_t>(mapAddr + cfg.offsetSize);
    uintptr_t head = 0;
    
    if (cfg.is64Bit)
        head = mem.Read<uintptr_t>(mapAddr + cfg.offsetHead);
    else
        head = (uintptr_t)mem.Read<uint32_t>(mapAddr + cfg.offsetHead);

    if (elementCount == 0 || elementCount > cfg.maxFlags) return false;
    if (head == 0 || head > (cfg.is64Bit ? 0x7FFFFFFF0000ULL : 0xFFFFFFFFU)) return false;

    // ── Bước 1: thu thập vài node mẫu đầu tiên để dò offsetNodeValue ──
    std::vector<uintptr_t>   sampleNodes;
    std::vector<std::string> sampleKeys;
    {
        uintptr_t cur = head;
        std::set<uintptr_t> visited;
        const size_t maxSamples = 20;   // đủ để chấm điểm tin cậy, không cần duyệt hết map

        while (cur != 0 && sampleNodes.size() < maxSamples) {
            if (visited.count(cur)) break;
            visited.insert(cur);

            std::string key = mem.ReadMsvcString(cur + cfg.offsetNodeKey);
            if (!key.empty() && key.size() >= 4) {
                sampleNodes.push_back(cur);
                sampleKeys.push_back(key);
            }

            uintptr_t next = cfg.is64Bit
                ? mem.Read<uintptr_t>(cur + cfg.offsetNodeNext)
                : (uintptr_t)mem.Read<uint32_t>(cur + cfg.offsetNodeNext);
            cur = next;
        }
    }

    if (sampleNodes.empty()) return false;

    size_t detectedValueOffset = DetectNodeValueOffset(mem, sampleNodes, sampleKeys, cfg);
    Log::Info("Detected node value offset: 0x" + ToHex(detectedValueOffset));

    // ── Bước 2: duyệt toàn bộ map bằng offset đã dò ──
    uintptr_t cur = head;
    size_t count  = 0;
    std::set<uintptr_t> visited;

    while (cur != 0 && count < elementCount) {
        if (visited.count(cur)) break;   // cycle guard
        visited.insert(cur);

        uintptr_t next = 0;
        if (cfg.is64Bit)
            next = mem.Read<uintptr_t>(cur + cfg.offsetNodeNext);
        else
            next = (uintptr_t)mem.Read<uint32_t>(cur + cfg.offsetNodeNext);

        // Read key (std::string)
        std::string key = mem.ReadMsvcString(cur + cfg.offsetNodeKey);
        if (!key.empty() && key.size() >= 4) {
            FlagValue val = ReadFlagValue(mem, cur + detectedValueOffset, key);
            out.push_back({key, val, cur});
        }

        cur = next;
        count++;
    }
    return count > 0;
}

