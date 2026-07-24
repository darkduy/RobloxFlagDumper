// RobloxFlagDumper.cpp  –  v4.0 (manual offsets, version-aware)
// FFlagList / ValueGetSet / FlagToValue được nhập thủ công (RVA từ base module)
// thay vì tự động dò tìm — tool chỉ lo phần duyệt map và xuất flag.
// Compile:  cl /O2 /MT /std:c++17 /EHsc /Fe:RobloxFlagDumper.exe RobloxFlagDumper.cpp src\*.cpp /link psapi.lib version.lib
// Run as Administrator

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <optional>

#include "src/Logger.h"
#include "src/Config.h"
#include "src/MemoryReader.h"
#include "src/Types.h"
#include "src/Utilities.h"
#include "src/MapTraversal.h"
#include "src/OutputWriter.h"

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "version.lib")

// ─────────────────────────── MANUAL OFFSET INPUT ─────────────────────────
// Đọc một offset dạng hex từ người dùng (RVA tính từ base module, ví dụ
// tìm được bằng IDA/Ghidra/x64dbg). Chấp nhận có hoặc không có tiền tố "0x".
// allowEmpty = true cho phép bỏ trống (dùng cho ValueGetSet, chỉ mang tính
// thông tin, không ảnh hưởng việc duyệt flag).
static std::optional<uintptr_t> PromptHexOffset(const std::string& label, bool allowEmpty)
{
    while (true) {
        std::cout << "  " << label
                   << (allowEmpty ? " (hex, Enter để bỏ qua): " : " (hex): ");
        std::string line;
        std::getline(std::cin, line);

        // Trim khoảng trắng
        size_t a = line.find_first_not_of(" \t\r\n");
        size_t b = line.find_last_not_of(" \t\r\n");
        line = (a == std::string::npos) ? "" : line.substr(a, b - a + 1);

        if (line.empty()) {
            if (allowEmpty) return std::nullopt;
            std::cout << "    -> Khong duoc bo trong, nhap lai.\n";
            continue;
        }
        if (line.size() > 2 && (line[0] == '0') && (line[1] == 'x' || line[1] == 'X'))
            line = line.substr(2);

        try {
            size_t consumed = 0;
            uintptr_t v = (uintptr_t)std::stoull(line, &consumed, 16);
            if (consumed != line.size()) throw std::invalid_argument("trailing chars");
            return v;
        } catch (...) {
            std::cout << "    -> Gia tri hex khong hop le (\"" << line << "\"), nhap lai.\n";
        }
    }
}

// ─────────────────────────── DIAGNOSTIC HELPER ───────────────────────────
// Đọc và in ra các field mà tool giả định là layout MSVC unordered_map
// tại mapAddr, kèm raw bytes, để đối chiếu thủ công khi TraverseMap thất bại
// hoặc để xác nhận offset đúng trước khi tin tưởng kết quả.
static void DiagnoseMapLayout(const MemoryReader& mem, uintptr_t mapAddr, const Config& cfg)
{
    Log::Step("Diagnostic: doc cac field gia dinh cua unordered_map tai mapAddr");

    size_t    bucketCount = mem.Read<size_t>(mapAddr + cfg.offsetBucketCount);
    uintptr_t head = cfg.is64Bit
        ? mem.Read<uintptr_t>(mapAddr + cfg.offsetHead)
        : (uintptr_t)mem.Read<uint32_t>(mapAddr + cfg.offsetHead);
    uintptr_t bucketArray = cfg.is64Bit
        ? mem.Read<uintptr_t>(mapAddr + cfg.offsetBucketArray)
        : (uintptr_t)mem.Read<uint32_t>(mapAddr + cfg.offsetBucketArray);
    size_t size = mem.Read<size_t>(mapAddr + cfg.offsetSize);

    Log::Detail("  _Bucket_count (+" + ToHex(cfg.offsetBucketCount) + ") = " + std::to_string(bucketCount));
    Log::Detail("  _Head         (+" + ToHex(cfg.offsetHead)        + ") = " + ToHex(head));
    Log::Detail("  _Buckets      (+" + ToHex(cfg.offsetBucketArray) + ") = " + ToHex(bucketArray));
    Log::Detail("  _Size         (+" + ToHex(cfg.offsetSize)        + ") = " + std::to_string(size));

    bool headOk = head != 0 && head < (cfg.is64Bit ? 0x7FFFFFFF0000ULL : 0xFFFFFFFFU);
    bool sizeOk = size > 0 && size < cfg.maxFlags;
    Log::Detail(std::string("  -> _Head hop le?  ") + (headOk ? "CO" : "KHONG (dia chi khong nam trong khong gian user-mode hop le)"));
    Log::Detail(std::string("  -> _Size hop le?  ") + (sizeOk ? "CO" : "KHONG (0, am, hoac vuot qua maxFlags = " + std::to_string(cfg.maxFlags) + ")"));

    uint8_t buf[0x40]{};
    if (mem.Read(mapAddr, buf, sizeof(buf))) {
        for (int row = 0; row < 4; row++) {
            std::ostringstream ss;
            ss << "  +" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (row * 16) << ": ";
            for (int col = 0; col < 16; col++)
                ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                   << (int)buf[row * 16 + col] << " ";
            Log::Detail(ss.str());
        }
    } else {
        Log::Detail("  (khong doc duoc raw bytes tai mapAddr -- dia chi co the khong hop le)");
    }

    if (!headOk || !sizeOk)
        Log::Warn("Layout tai mapAddr khong khop unordered_map MSVC -- FFlagList RVA co le sai. Xem huong dan xac minh lai bang debugger.");

    // Kiem tra truc tiep: doc thu node dau tien tai _Head, bat ke _Size co
    // hop le hay khong. Neu key giai ma ra ten flag that -> _Head va layout
    // node (offsetNodeKey/offsetNodeNext...) DUNG, du _Size sai o dau cung duoc.
    if (head) {
        Log::Detail("  -- Thu doc node dau tien tai _Head --");
        uint8_t nodeBuf[0x40]{};
        if (mem.Read(head, nodeBuf, sizeof(nodeBuf))) {
            for (int row = 0; row < 4; row++) {
                std::ostringstream ss;
                ss << "  head+" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (row * 16) << ": ";
                for (int col = 0; col < 16; col++)
                    ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                       << (int)nodeBuf[row * 16 + col] << " ";
                Log::Detail(ss.str());
            }
        }
        std::string key = mem.ReadMsvcString(head + cfg.offsetNodeKey);
        if (!key.empty())
            Log::Detail("  -> Key doc duoc tai head+offsetNodeKey (" + ToHex(cfg.offsetNodeKey) + "): \"" + key
                      + "\"  --  neu giong ten flag that, _Head va node layout DUNG.");
        else
            Log::Detail("  -> Khong doc duoc key hop le tai head+offsetNodeKey -- offsetNodeKey co the sai, hoac head khong phai node that.");
    }
}

// ─────────────────────────────── MAIN ────────────────────────────────────
int main()
{
    Log::Init("Roblox Fast Flag Dumper v4.0 (Manual Offsets)");

    // 1. Find Roblox process
    Log::Step("Finding Roblox process");
    DWORD pid = 0;
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(snap, &pe)) {
            do {
                if (wcscmp(pe.szExeFile, L"RobloxPlayerBeta.exe") == 0 ||
                    wcscmp(pe.szExeFile, L"RobloxPlayer.exe") == 0) {
                    pid = pe.th32ProcessID;
                    std::wstring exeName(pe.szExeFile);
                    Log::Info("Found: " + std::string(exeName.begin(), exeName.end()));
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    if (!pid) {
        Log::Err("Roblox process not found. Is Roblox running?");
        return 1;
    }
    Log::Info("PID: " + std::to_string(pid));

    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) {
        Log::Err("OpenProcess failed (error " + std::to_string(GetLastError()) + "). Run as Administrator.");
        return 1;
    }
    Log::Info("Process opened successfully.");

    // 2. Detect process architecture
    Log::Step("Detecting process architecture");
    BOOL isWow64 = FALSE;
    IsWow64Process(hProc, &isWow64);
    bool is64Bit = !isWow64;
    Log::Info("Process type: " + std::string(is64Bit ? "64-bit" : "32-bit"));

    // 3. Load configuration (layout offsets cho std::string / unordered_map node)
    std::string version = GetRobloxVersion(hProc);
    Config cfg = LoadOrCreateConfig(version, is64Bit);
    Log::Info("Configuration loaded for: " + cfg.version);

    MemoryReader mem(hProc, &cfg);

    // 4. Get base address
    Log::Step("Reading module info");
    HMODULE hMods[1];
    DWORD   cbNeeded;
    if (!EnumProcessModules(hProc, hMods, sizeof(hMods), &cbNeeded)) {
        Log::Err("EnumProcessModules failed (error " + std::to_string(GetLastError()) + ").");
        CloseHandle(hProc); return 1;
    }
    uintptr_t base = (uintptr_t)hMods[0];
    MODULEINFO mi;
    GetModuleInformation(hProc, hMods[0], &mi, sizeof(mi));
    Log::Info("Base address : " + ToHex(base));
    Log::Info("Image size   : " + ToHex(mi.SizeOfImage) + " (" + std::to_string(mi.SizeOfImage / 1024 / 1024) + " MB)");

    // 5. Nhập offset thủ công (RVA tính từ base module)
    Log::Step("Nhap offset thu cong (RVA tu base module)");
    Log::Info("Roblox version: " + version + " -- offset phai khop dung ban nay.");
    uintptr_t offFFlagList  = PromptHexOffset("FFlagList  ", false).value();
    uintptr_t offFlagToValue = PromptHexOffset("FlagToValue", false).value();
    std::optional<uintptr_t> offValueGetSet = PromptHexOffset("ValueGetSet", true);

    uintptr_t fflagListPtr = base + offFFlagList;
    uintptr_t flagToValue  = base + offFlagToValue;
    uintptr_t getterFunc   = offValueGetSet ? (base + *offValueGetSet) : 0;

    Log::Info("FFlagList  ptr addr : " + ToHex(fflagListPtr));
    Log::Info("FlagToValue ptr addr: " + ToHex(flagToValue));
    Log::Info("ValueGetSet addr    : " + (getterFunc ? ToHex(getterFunc) : std::string("0x0 (bo qua)")));

    // 6. Dereference FFlagList -> địa chỉ map thực sự
    uintptr_t mapAddr = cfg.is64Bit
        ? mem.Read<uintptr_t>(fflagListPtr)
        : (uintptr_t)mem.Read<uint32_t>(fflagListPtr);

    if (!mapAddr) {
        Log::Err("Doc FFlagList ra 0x0 -- offset sai hoac Roblox chua load xong. Kiem tra lai va thu lai.");
        CloseHandle(hProc); return 1;
    }
    Log::Info("FFlagList  ->  map @ " + ToHex(mapAddr));

    DiagnoseMapLayout(mem, mapAddr, cfg);

    // 7. Traverse map
    Log::Step("Traversing FFlagList map");
    std::vector<FlagEntry> flags;
    if (!TraverseMap(mem, mapAddr, flags, cfg)) {
        Log::Err("Map traversal that bai. Offset FFlagList co the sai, hoac layout node (Config) khong khop ban Roblox nay.");
        CloseHandle(hProc); return 1;
    }
    Log::Info("Raw nodes read: " + std::to_string(flags.size()));

    std::sort(flags.begin(), flags.end(), [](const FlagEntry& a, const FlagEntry& b) {
        return a.name < b.name;
    });

    size_t nBool = 0, nInt = 0, nStr = 0, nUnk = 0;
    for (const auto& f : flags) {
        switch (f.value.type) {
            case FlagType::Bool:   nBool++; break;
            case FlagType::Int:    nInt++;  break;
            case FlagType::String: nStr++;  break;
            default:               nUnk++;  break;
        }
    }
    Log::Info("Type breakdown  ->  bool=" + std::to_string(nBool)
            + "  int=" + std::to_string(nInt)
            + "  string=" + std::to_string(nStr)
            + "  unknown=" + std::to_string(nUnk));

    Log::Detail("Sample flags:");
    size_t sampleCount = flags.size() < 5 ? flags.size() : 5;
    for (size_t i = 0; i < sampleCount; i++) {
        Log::Detail("  " + flags[i].name + " = " + flags[i].value.ToString()
                  + "  @ " + ToHex(flags[i].nodeAddr));
    }

    // 8. Output
    Log::Step("Writing output files");
    Log::Info("Roblox version: " + version);

    WriteHpp(flags, fflagListPtr, getterFunc, flagToValue, version, "RobloxFlags.hpp");
    Log::Info("RobloxFlags.hpp written  (" + std::to_string(flags.size()) + " flags)");
    Log::Info("dumper.log      written");

    CloseHandle(hProc);

    std::cout << "\n";
    Log::Info("All done. Exiting.");
    return 0;
}