// RobloxFlagDumper.cpp  –  v2.0 (fully dynamic, educational only)
// Compile:  g++ -O2 -static RobloxFlagDumper.cpp -o RobloxFlagDumper.exe
// Run as Administrator
//
// Cải tiến so với v1.0:
//   [+] Parse PE header để tách .text / .rdata / .data riêng biệt
//   [+] Quét từng section đúng vai trò (code vs data)
//   [+] Xác định value type thực (bool / int / string) thay vì luôn dùng bool
//   [+] Multi-map voting – giảm false positive khi chọn FFlagList
//   [+] Heuristic scoring mạnh hơn (prime bucket count, load factor)
//   [+] Xuất song song JSON + HPP (flags.json + RobloxFlags.hpp)
//   [+] Progress bar đơn giản
//   [+] Kiểm tra hợp lệ string UTF-8 / ASCII

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <ctime>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "version.lib")

// ─────────────────────────────── CONSTANTS ───────────────────────────────
static constexpr size_t kMaxFlags      = 150'000;
static constexpr size_t kMinElements   = 100;
static constexpr size_t kMaxBuckets    = 2'000'000;
static constexpr size_t kMaxStrLen     = 128;
static constexpr size_t kReadChunk     = 0x1000;

// ─────────────────────────────── UTILITIES ───────────────────────────────
static std::string ToHex(uintptr_t v)
{
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << v;
    return ss.str();
}

static bool IsPrintableAscii(const std::string& s)
{
    for (unsigned char c : s)
        if (c < 0x20 || c > 0x7E) return false;
    return true;
}

// Kiểm tra xem một số có phải là số nguyên tố không (dùng để đánh giá bucket count)
static bool IsPrime(size_t n)
{
    if (n < 2) return false;
    if (n < 4) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (size_t i = 5; i * i <= n; i += 6)
        if (n % i == 0 || n % (i + 2) == 0) return false;
    return true;
}

// ─────────────────────────────── MEMORY READER ───────────────────────────
class MemoryReader
{
    HANDLE hProc;
public:
    explicit MemoryReader(HANDLE h) : hProc(h) {}

    bool Read(uintptr_t addr, void* buf, size_t sz) const
    {
        SIZE_T got;
        return ReadProcessMemory(hProc, (LPCVOID)addr, buf, sz, &got) && got == sz;
    }

    template<typename T>
    T Read(uintptr_t addr) const
    {
        T v{};
        Read(addr, &v, sizeof(v));
        return v;
    }

    // Đọc std::string MSVC SSO-aware:
    //   +0x00  char* _Ptr  (hoặc inline buf[16])
    //   +0x10  size_t _Size
    //   +0x18  size_t _Res
    std::string ReadMsvcString(uintptr_t addr) const
    {
        // SSO threshold = 15 chars inline
        size_t sz  = Read<size_t>(addr + 0x10);
        size_t res = Read<size_t>(addr + 0x18);
        if (sz == 0 || sz > kMaxStrLen) return {};

        char buf[kMaxStrLen + 1]{};
        if (res < 16) {
            // inline storage: data bắt đầu ở addr+0x00 (16-byte union)
            Read(addr, buf, std::min(sz, (size_t)15));
        } else {
            uintptr_t ptr = Read<uintptr_t>(addr);
            if (!ptr || ptr < 0x10000) return {};
            if (!Read(ptr, buf, sz)) return {};
        }
        buf[sz] = '\0';
        std::string s(buf, sz);
        return IsPrintableAscii(s) ? s : std::string{};
    }

    // Đọc C-string tại địa chỉ thô
    std::string ReadCString(uintptr_t addr, size_t maxLen = kMaxStrLen) const
    {
        char buf[kMaxStrLen + 1]{};
        if (!Read(addr, buf, maxLen)) return {};
        buf[maxLen] = '\0';
        size_t len = strnlen(buf, maxLen);
        std::string s(buf, len);
        return IsPrintableAscii(s) ? s : std::string{};
    }
};

// ─────────────────────────────── PE SECTIONS ─────────────────────────────
struct Section {
    std::string name;
    uintptr_t   va;       // virtual address (absolute)
    size_t      size;
    uint32_t    chars;    // IMAGE_SCN_*
};

std::vector<Section> ParsePeSections(const MemoryReader& mem, uintptr_t base)
{
    std::vector<Section> secs;
    uint8_t  dosHdr[0x40];
    if (!mem.Read(base, dosHdr, sizeof(dosHdr))) return secs;
    if (dosHdr[0] != 'M' || dosHdr[1] != 'Z') return secs;

    uint32_t peOff = *(uint32_t*)(dosHdr + 0x3C);
    uint8_t  ntHdr[0x108];
    if (!mem.Read(base + peOff, ntHdr, sizeof(ntHdr))) return secs;
    if (ntHdr[0] != 'P' || ntHdr[1] != 'E') return secs;

    uint16_t numSec   = *(uint16_t*)(ntHdr + 6);
    uint16_t optSize  = *(uint16_t*)(ntHdr + 20);
    uintptr_t secHdrOff = base + peOff + 24 + optSize;

    for (uint16_t i = 0; i < numSec; i++) {
        uint8_t sh[40];
        if (!mem.Read(secHdrOff + i * 40, sh, 40)) break;
        Section s;
        s.name  = std::string((char*)sh, strnlen((char*)sh, 8));
        s.va    = base + *(uint32_t*)(sh + 12);
        s.size  = *(uint32_t*)(sh + 16);
        s.chars = *(uint32_t*)(sh + 36);
        secs.push_back(s);
    }
    return secs;
}

// IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE
static constexpr uint32_t kSecCode = 0x00000020 | 0x20000000;
// IMAGE_SCN_CNT_INITIALIZED_DATA
static constexpr uint32_t kSecData = 0x00000040;

// ─────────────────────────── INSTRUCTION HELPERS ─────────────────────────
// lea rdx, [rip+disp32]  → 48 8D 15 ?? ?? ?? ??
// lea rcx, [rip+disp32]  → 48 8D 0D ?? ?? ?? ??
// mov rcx, [rip+disp32]  → 48 8B 0D ?? ?? ?? ??
static inline bool IsLeaRdxRip(const uint8_t* b) { return b[0]==0x48 && b[1]==0x8D && b[2]==0x15; }
static inline bool IsLeaRcxRip(const uint8_t* b) { return b[0]==0x48 && b[1]==0x8D && b[2]==0x0D; }
static inline bool IsMovRcxRip(const uint8_t* b) { return b[0]==0x48 && b[1]==0x8B && b[2]==0x0D; }
static inline bool IsMovRaxRip(const uint8_t* b) { return b[0]==0x48 && b[1]==0x8B && b[2]==0x05; }

static inline uintptr_t ResolveRip(uintptr_t instrAddr, const uint8_t* bytes)
{
    int32_t disp = *(int32_t*)(bytes + 3);
    return instrAddr + 7 + disp;
}

// ─────────────────────────────── FLAG VALUE TYPE ──────────────────────────
enum class FlagType { Bool, Int, String, Unknown };

struct FlagValue {
    FlagType    type;
    bool        boolVal;
    int64_t     intVal;
    std::string strVal;

    std::string ToString() const {
        switch (type) {
            case FlagType::Bool:   return boolVal ? "true" : "false";
            case FlagType::Int:    return std::to_string(intVal);
            case FlagType::String: return "\"" + strVal + "\"";
            default:               return "unknown";
        }
    }
    std::string TypeName() const {
        switch (type) {
            case FlagType::Bool:   return "bool";
            case FlagType::Int:    return "int";
            case FlagType::String: return "string";
            default:               return "unknown";
        }
    }
};

// Đọc value từ node; node+0x28 = value union
// Heuristic: đọc 8 bytes và suy luận kiểu dựa trên tên flag
FlagValue ReadFlagValue(const MemoryReader& mem, uintptr_t nodeAddr, const std::string& name)
{
    FlagValue fv;
    // Đoán kiểu từ tiền tố tên
    bool isBool   = (name.rfind("FFlag",  0) == 0 || name.rfind("DFFlag", 0) == 0);
    bool isInt    = (name.rfind("FInt",   0) == 0 || name.rfind("DFInt",  0) == 0);
    bool isString = (name.rfind("FString",0) == 0 || name.rfind("DFString",0)== 0);

    uintptr_t valOff = nodeAddr + 0x28;

    if (isInt) {
        fv.type   = FlagType::Int;
        fv.intVal = mem.Read<int64_t>(valOff);
    } else if (isString) {
        fv.type   = FlagType::String;
        fv.strVal = mem.ReadMsvcString(valOff);
    } else {
        // Default: bool (FFlag / DFFlag / SFFlag)
        uint8_t b = mem.Read<uint8_t>(valOff);
        fv.type    = FlagType::Bool;
        fv.boolVal = (b != 0);
    }
    (void)isBool;
    (void)isString;
    return fv;
}

// ─────────────────────────── MAP CANDIDATE ───────────────────────────────
struct MapCandidate {
    uintptr_t address;
    size_t    bucketCount;
    size_t    elementCount;
    uintptr_t bucketArray;
    uintptr_t firstNode;
    float     score;
};

// Layout của MSVC std::unordered_map<std::string, T> (x64):
//   +0x00  size_t  _Bucket_count    (số bucket, thường là số nguyên tố)
//   +0x08  _List_node* _Head        (con trỏ đến node đầu tiên trong linked list)
//   +0x10  _Bucket_array* _Buckets  (mảng các con trỏ)
//   +0x18  size_t  _Size            (số phần tử)
//   +0x20  float   _Max_load_factor (thường = 1.0f → 0x3F800000 trong memory)
std::vector<MapCandidate> FindMapCandidates(
    const MemoryReader& mem,
    const std::vector<Section>& sections)
{
    std::vector<MapCandidate> out;

    for (const auto& sec : sections) {
        // Tìm trong .data và .bss (data sections, không phải code)
        if (sec.chars & kSecCode) continue;
        if (!(sec.chars & kSecData)) continue;

        uint8_t buf[kReadChunk];
        for (uintptr_t addr = sec.va; addr < sec.va + sec.size; addr += kReadChunk - 0x30) {
            if (!mem.Read(addr, buf, sizeof(buf))) continue;

            for (size_t off = 0; off + 0x28 <= sizeof(buf); off += 8) {
                uintptr_t objAddr    = addr + off;
                size_t bucketCount   = *(size_t*)(buf + off);
                uintptr_t firstNode  = *(uintptr_t*)(buf + off + 0x08);
                uintptr_t bucketArr  = *(uintptr_t*)(buf + off + 0x10);
                size_t elementCount  = *(size_t*)(buf + off + 0x18);
                uint32_t loadFactor  = *(uint32_t*)(buf + off + 0x20);

                // Sơ lọc nhanh
                if (elementCount < kMinElements || elementCount > kMaxFlags) continue;
                if (bucketCount  < elementCount || bucketCount > kMaxBuckets) continue;
                if (bucketArr < 0x10000 || bucketArr > 0x7FFFFFFF0000ULL) continue;

                float score = 0.0f;

                // Load factor == 1.0f (IEEE 754: 0x3F800000)
                if (loadFactor == 0x3F800000u) score += 20.0f;

                // Bucket count là số nguyên tố → MSVC thích dùng prime
                if (IsPrime(bucketCount)) score += 15.0f;

                // Nhiều phần tử hơn thì ưu tiên
                if (elementCount > 5000)  score += 10.0f;
                if (elementCount > 10000) score += 10.0f;

                // firstNode hợp lệ
                if (firstNode > 0x10000 && firstNode < 0x7FFFFFFF0000ULL) score += 10.0f;

                // Load factor hợp lý (elementCount / bucketCount <= 1.0)
                float lf = (float)elementCount / (float)bucketCount;
                if (lf > 0.3f && lf <= 1.0f) score += 5.0f;

                out.push_back({objAddr, bucketCount, elementCount, bucketArr, firstNode, score});
            }
        }
    }

    std::sort(out.begin(), out.end(), [](const MapCandidate& a, const MapCandidate& b) {
        return a.score > b.score;
    });

    // Loại bỏ trùng lặp (địa chỉ quá gần nhau)
    std::vector<MapCandidate> dedup;
    for (const auto& c : out) {
        bool dup = false;
        for (const auto& d : dedup)
            if (std::abs((intptr_t)(c.address - d.address)) < 0x100) { dup = true; break; }
        if (!dup) dedup.push_back(c);
    }
    return dedup;
}

// ─────────────────────────── MAP TRAVERSAL ───────────────────────────────
// Node layout MSVC unordered_map (x64, std::string key):
//   +0x00  _Node* _Next
//   +0x08  size_t _Hash_code (cached)
//   +0x10  std::string _Kval  (key, 32 bytes: ptr/inline, size, res)
//   +0x30  Mapped _Myval      (value)
struct FlagEntry {
    std::string name;
    FlagValue   value;
    uintptr_t   nodeAddr;  // địa chỉ node trong process memory
};

bool TraverseMap(
    const MemoryReader& mem,
    uintptr_t mapAddr,
    std::vector<FlagEntry>& out)
{
    size_t elementCount  = mem.Read<size_t>(mapAddr + 0x18);
    uintptr_t head       = mem.Read<uintptr_t>(mapAddr + 0x08);

    if (elementCount == 0 || elementCount > kMaxFlags) return false;
    if (head == 0 || head > 0x7FFFFFFF0000ULL) return false;

    uintptr_t cur = head;
    size_t count  = 0;
    std::set<uintptr_t> visited;

    while (cur != 0 && count < elementCount) {
        if (visited.count(cur)) break;   // cycle guard
        visited.insert(cur);

        uintptr_t next = mem.Read<uintptr_t>(cur);

        // Key là std::string tại +0x10
        std::string key = mem.ReadMsvcString(cur + 0x10);
        if (!key.empty() && key.size() >= 4) {
            FlagValue val = ReadFlagValue(mem, cur, key);
            out.push_back({key, val, cur});
        }

        cur = next;
        count++;
    }
    return count > 0;
}

// ─────────────────────────── FLAG STRING SCAN ────────────────────────────
struct FlagRef {
    uintptr_t   stringAddr;
    std::string name;
};

std::vector<FlagRef> ScanFlagStrings(
    const MemoryReader& mem,
    const std::vector<Section>& sections)
{
    std::vector<FlagRef> refs;
    uint8_t buf[kReadChunk];

    // Tiền tố cần tìm
    static const char* prefixes[] = { "FFlag", "DFFlag", "SFFlag", "FInt", "DFInt", "FString", nullptr };

    for (const auto& sec : sections) {
        // Strings thường nằm trong .rdata (read-only data)
        if (sec.chars & kSecCode) continue;

        for (uintptr_t addr = sec.va; addr < sec.va + sec.size; addr += kReadChunk - 8) {
            if (!mem.Read(addr, buf, sizeof(buf))) continue;

            for (size_t i = 0; i + 8 <= sizeof(buf); i++) {
                for (int pi = 0; prefixes[pi]; pi++) {
                    size_t plen = strlen(prefixes[pi]);
                    if (i + plen > sizeof(buf)) continue;
                    if (memcmp(buf + i, prefixes[pi], plen) != 0) continue;

                    // Đo độ dài
                    size_t len = 0;
                    while (i + len < sizeof(buf) && buf[i + len] != '\0') len++;
                    if (len < plen || len >= kMaxStrLen) continue;

                    std::string name((char*)buf + i, len);
                    if (!IsPrintableAscii(name)) continue;

                    refs.push_back({ addr + i, name });
                    i += len;
                    break;
                }
            }
        }
    }

    // Loại bỏ trùng (cùng tên)
    std::sort(refs.begin(), refs.end(), [](const FlagRef& a, const FlagRef& b) {
        return a.name < b.name;
    });
    refs.erase(std::unique(refs.begin(), refs.end(), [](const FlagRef& a, const FlagRef& b) {
        return a.name == b.name;
    }), refs.end());

    return refs;
}

// ─────────────────────────── GETTER FUNCTION FINDER ──────────────────────
uintptr_t FindGetterFunction(
    const MemoryReader& mem,
    const std::vector<FlagRef>& flagRefs,
    const std::vector<Section>& codeSections)
{
    // Lấy các section có code
    std::vector<std::pair<uintptr_t,size_t>> codeRanges;
    for (const auto& s : codeSections)
        if (s.chars & kSecCode)
            codeRanges.push_back({s.va, s.size});

    std::map<uintptr_t, int> funcHits;
    uint8_t buf[kReadChunk];

    size_t total = flagRefs.size();
    size_t done  = 0;

    for (const auto& ref : flagRefs) {
        done++;
        if (done % 200 == 0)
            std::cout << "\r  Getter scan: " << done << "/" << total << "   " << std::flush;

        for (const auto& [rangeStart, rangeSize] : codeRanges) {
            for (uintptr_t addr = rangeStart; addr < rangeStart + rangeSize; addr += kReadChunk - 7) {
                if (!mem.Read(addr, buf, sizeof(buf))) continue;
                for (size_t i = 0; i + 7 <= sizeof(buf); i++) {
                    // lea rdx, [rip+disp] hoặc lea rcx, [rip+disp]
                    if (!IsLeaRdxRip(buf+i) && !IsLeaRcxRip(buf+i)) continue;
                    uintptr_t target = ResolveRip(addr + i, buf + i);
                    if (target != ref.stringAddr) continue;

                    // Cố tìm prologue ngược (tối đa 512 bytes)
                    uintptr_t instrAddr = addr + i;
                    for (uintptr_t scan = instrAddr - 2; scan > instrAddr - 512; scan--) {
                        uint8_t prologue[6];
                        if (!mem.Read(scan, prologue, 6)) break;
                        // Prologue phổ biến x64 MSVC: 40 53 / 41 54 / 48 83 EC
                        bool isPrologue =
                            (prologue[0] == 0x40 && (prologue[1] == 0x53 || prologue[1] == 0x55)) ||
                            (prologue[0] == 0x41 && (prologue[1] == 0x54 || prologue[1] == 0x56)) ||
                            (prologue[0] == 0x48 && prologue[1] == 0x83 && prologue[2] == 0xEC);
                        if (isPrologue) { funcHits[scan]++; break; }
                        // Dừng nếu gặp INT3 hoặc RET
                        if (prologue[0] == 0xCC || prologue[0] == 0xC3) break;
                    }
                }
            }
        }
    }
    std::cout << "\r  Getter scan: done          \n";

    uintptr_t best = 0; int maxHits = 0;
    for (const auto& [fn, hits] : funcHits)
        if (hits > maxHits) { maxHits = hits; best = fn; }
    return best;
}

// ─────────────────────── GLOBAL MAP POINTER FINDER ───────────────────────
// Multi-map voting: đếm số lần mỗi map candidate được tham chiếu từ code.
// Trả về struct chứa cả FFlagList pointer và FlagToValue pointer riêng biệt.
struct GlobalPtrs {
    uintptr_t fflagListPtr   = 0;
    uintptr_t flagToValuePtr = 0;
};

GlobalPtrs FindGlobalMapPointers(
    const MemoryReader& mem,
    const std::vector<MapCandidate>& maps,
    const std::vector<Section>& codeSections)
{
    // votes[ptrAddr] = { mapCandidateIndex, hitCount }
    std::map<uintptr_t, std::pair<size_t,int>> votes;
    uint8_t buf[kReadChunk];

    std::vector<std::pair<uintptr_t,size_t>> codeRanges;
    for (const auto& s : codeSections)
        if (s.chars & kSecCode) codeRanges.push_back({s.va, s.size});

    for (const auto& [rangeStart, rangeSize] : codeRanges) {
        for (uintptr_t addr = rangeStart; addr < rangeStart + rangeSize; addr += kReadChunk - 7) {
            if (!mem.Read(addr, buf, sizeof(buf))) continue;
            for (size_t i = 0; i + 7 <= sizeof(buf); i++) {
                if (!IsMovRcxRip(buf+i) && !IsMovRaxRip(buf+i)) continue;
                uintptr_t ptrAddr = ResolveRip(addr + i, buf + i);
                uintptr_t mapAddr = mem.Read<uintptr_t>(ptrAddr);
                for (size_t mi = 0; mi < maps.size(); mi++) {
                    if (maps[mi].address == mapAddr) {
                        votes[ptrAddr].first  = mi;
                        votes[ptrAddr].second++;
                        break;
                    }
                }
            }
        }
    }

    // Map được tham chiếu nhiều nhất → FFlagList
    // Map được tham chiếu nhiều thứ 2 (khác map đầu) → FlagToValue
    uintptr_t bestPtr1 = 0, bestPtr2 = 0;
    int maxV1 = 0, maxV2 = 0;
    size_t mapIdx1 = SIZE_MAX;

    for (const auto& [ptr, p] : votes) {
        if (p.second > maxV1) {
            maxV2 = maxV1; bestPtr2 = bestPtr1;
            maxV1 = p.second; bestPtr1 = ptr; mapIdx1 = p.first;
        } else if (p.second > maxV2 && p.first != mapIdx1) {
            maxV2 = p.second; bestPtr2 = ptr;
        }
    }

    return { bestPtr1, bestPtr2 };
}

// ─────────────────────────────── JSON OUTPUT ─────────────────────────────
static std::string JsonEscape(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else               out += c;
    }
    return out;
}

void WriteJson(const std::vector<FlagEntry>& flags, const std::string& path)
{
    std::ofstream f(path);
    f << "{\n";
    for (size_t i = 0; i < flags.size(); i++) {
        const auto& fe = flags[i];
        f << "  \"" << JsonEscape(fe.name) << "\": {\"type\":\"" << fe.value.TypeName()
          << "\", \"value\": ";
        if (fe.value.type == FlagType::String)
            f << "\"" << JsonEscape(fe.value.strVal) << "\"";
        else if (fe.value.type == FlagType::Bool)
            f << (fe.value.boolVal ? "true" : "false");
        else
            f << fe.value.intVal;
        f << "}";
        if (i + 1 < flags.size()) f << ",";
        f << "\n";
    }
    f << "}\n";
}

// Lấy version hash từ file version info của process (ProductVersion / FileVersion)
std::string GetRobloxVersion(HANDLE hProc)
{
    HMODULE hMods[1];
    DWORD cb;
    wchar_t path[MAX_PATH];
    if (!EnumProcessModules(hProc, hMods, sizeof(hMods), &cb)) return "unknown";
    if (!GetModuleFileNameExW(hProc, hMods[0], path, MAX_PATH))  return "unknown";

    DWORD  dummy;
    DWORD  infoSize = GetFileVersionInfoSizeW(path, &dummy);
    if (!infoSize) return "unknown";

    std::vector<uint8_t> buf(infoSize);
    if (!GetFileVersionInfoW(path, 0, infoSize, buf.data())) return "unknown";

    VS_FIXEDFILEINFO* ffi = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(buf.data(), L"\\", (LPVOID*)&ffi, &len) || !ffi) return "unknown";

    char ver[64];
    snprintf(ver, sizeof(ver), "version-%u-%u-%u-%u",
        HIWORD(ffi->dwFileVersionMS), LOWORD(ffi->dwFileVersionMS),
        HIWORD(ffi->dwFileVersionLS), LOWORD(ffi->dwFileVersionLS));
    return std::string(ver);
}

void WriteHpp(
    const std::vector<FlagEntry>& flags,
    uintptr_t fflagListPtr,
    uintptr_t getterFunc,
    uintptr_t flagToValue,
    const std::string& version,
    const std::string& path)
{
    std::ofstream f(path);

    time_t now = time(nullptr);
    char tbuf[64];
    tm tm_info;
    localtime_s(&tm_info, &now);
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm_info);

    // Sanitize identifier: thay ký tự không hợp lệ C++ bằng '_'
    auto sanitize = [](const std::string& s) {
        std::string r = s;
        for (char& c : r)
            if (!isalnum((unsigned char)c) && c != '_') c = '_';
        // Không được bắt đầu bằng chữ số
        if (!r.empty() && isdigit((unsigned char)r[0])) r = "_" + r;
        return r;
    };

    f << "// Roblox Version - " << version << "\n"
      << "// Total flags: " << flags.size() << "\n"
      << "// Dumped by RobloxFlagDumper at " << tbuf << "\n"
      << "#pragma once\n"
      << "namespace FFlagOffsets {\n"
      << "    uintptr_t FFlagList  = " << ToHex(fflagListPtr) << ";\n"
      << "    uintptr_t ValueGetSet = " << ToHex(getterFunc)   << ";\n"
      << "    uintptr_t FlagToValue = " << ToHex(flagToValue)  << ";\n"
      << "}\n"
      << "namespace FFlags {\n";

    for (const auto& fe : flags)
        f << "    uintptr_t " << sanitize(fe.name) << " = " << ToHex(fe.nodeAddr) << ";\n";

    f << "}\n";
}

// ─────────────────────────────── MAIN ────────────────────────────────────
int main()
{
    std::cout << "=== Roblox Fast Flag Dumper v2.0 ===\n\n";

    // 1. Tìm process
    DWORD pid = 0;
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(snap, &pe)) {
            do {
                if (wcscmp(pe.szExeFile, L"RobloxPlayerBeta.exe") == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    if (!pid) { std::cerr << "[!] RobloxPlayerBeta.exe not found.\n"; return 1; }
    std::cout << "[+] PID: " << pid << "\n";

    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) { std::cerr << "[!] OpenProcess failed. Run as Administrator.\n"; return 1; }

    MemoryReader mem(hProc);

    // 2. Lấy base address
    HMODULE hMods[1024];
    DWORD   cbNeeded;
    if (!EnumProcessModules(hProc, hMods, sizeof(hMods), &cbNeeded)) {
        std::cerr << "[!] EnumProcessModules failed.\n";
        CloseHandle(hProc); return 1;
    }
    uintptr_t base = (uintptr_t)hMods[0];
    MODULEINFO mi;
    GetModuleInformation(hProc, hMods[0], &mi, sizeof(mi));
    std::cout << "[+] Base: " << ToHex(base) << "  Size: " << ToHex(mi.SizeOfImage) << "\n";

    // 3. Parse PE sections
    auto sections = ParsePeSections(mem, base);
    if (sections.empty()) { std::cerr << "[!] Could not parse PE sections.\n"; CloseHandle(hProc); return 1; }
    std::cout << "[+] Sections found: " << sections.size() << "\n";
    for (const auto& s : sections)
        std::cout << "    " << std::left << std::setw(10) << s.name
                  << "  VA=" << ToHex(s.va)
                  << "  size=" << ToHex(s.size)
                  << "  chars=" << ToHex(s.chars) << "\n";

    // 4. Quét flag strings
    std::cout << "\n[*] Scanning for FFlag strings...\n";
    auto flagRefs = ScanFlagStrings(mem, sections);
    std::cout << "[+] FFlag strings found: " << flagRefs.size() << "\n";
    if (flagRefs.empty()) { std::cerr << "[!] No FFlag strings – Roblox may have changed encoding.\n"; CloseHandle(hProc); return 1; }

    // 5. Tìm getter function
    std::cout << "\n[*] Locating getter function...\n";
    uintptr_t getterFunc = FindGetterFunction(mem, flagRefs, sections);
    std::cout << (getterFunc ? "[+] Getter: " + ToHex(getterFunc) : "[~] Getter not found") << "\n";

    // 6. Tìm map candidates
    std::cout << "\n[*] Searching for unordered_map candidates...\n";
    auto mapCandidates = FindMapCandidates(mem, sections);
    std::cout << "[+] Candidates: " << mapCandidates.size() << "\n";
    for (size_t i = 0; i < std::min((size_t)5, mapCandidates.size()); i++) {
        const auto& c = mapCandidates[i];
        std::cout << "    [" << i << "] addr=" << ToHex(c.address)
                  << "  elems=" << c.elementCount
                  << "  buckets=" << c.bucketCount
                  << "  score=" << c.score << "\n";
    }
    if (mapCandidates.empty()) { std::cerr << "[!] No map candidates.\n"; CloseHandle(hProc); return 1; }

    // 7. Multi-map voting để tìm FFlagList + FlagToValue
    std::cout << "\n[*] Voting for global map pointers...\n";
    auto gptrs = FindGlobalMapPointers(mem, mapCandidates, sections);

    uintptr_t fflagListPtr  = gptrs.fflagListPtr;
    uintptr_t flagToValue   = gptrs.flagToValuePtr;
    uintptr_t mapAddr       = 0;

    if (fflagListPtr) {
        mapAddr = mem.Read<uintptr_t>(fflagListPtr);
        std::cout << "[+] FFlagList  ptr: " << ToHex(fflagListPtr) << " -> map: " << ToHex(mapAddr) << "\n";
    } else {
        mapAddr = mapCandidates[0].address;
        std::cout << "[~] FFlagList fallback: best candidate at " << ToHex(mapAddr) << "\n";
    }
    if (flagToValue)
        std::cout << "[+] FlagToValue ptr: " << ToHex(flagToValue) << " -> map: " << ToHex(mem.Read<uintptr_t>(flagToValue)) << "\n";
    else
        std::cout << "[~] FlagToValue not found\n";

    // 8. Traverse map
    std::cout << "\n[*] Traversing map...\n";
    std::vector<FlagEntry> flags;
    if (!TraverseMap(mem, mapAddr, flags)) {
        std::cerr << "[!] Traverse failed – offsets may be wrong.\n";
        CloseHandle(hProc); return 1;
    }
    std::cout << "[+] Dumped " << flags.size() << " flags.\n";

    // Sort by name
    std::sort(flags.begin(), flags.end(), [](const FlagEntry& a, const FlagEntry& b) {
        return a.name < b.name;
    });

    // Stats
    size_t nBool = 0, nInt = 0, nStr = 0, nUnk = 0;
    for (const auto& f : flags) {
        switch (f.value.type) {
            case FlagType::Bool:   nBool++; break;
            case FlagType::Int:    nInt++;  break;
            case FlagType::String: nStr++;  break;
            default:               nUnk++;  break;
        }
    }
    std::cout << "    bool=" << nBool << "  int=" << nInt << "  string=" << nStr << "  unknown=" << nUnk << "\n";

    // 10. Output
    std::cout << "\n[*] Writing outputs...\n";
    std::string version = GetRobloxVersion(hProc);
    std::cout << "[+] Roblox version: " << version << "\n";
    WriteHpp(flags, fflagListPtr, getterFunc, flagToValue, version, "RobloxFlags.hpp");
    std::cout << "[+] RobloxFlags.hpp written (" << flags.size() << " entries)\n";

    CloseHandle(hProc);
    std::cout << "\n[*] Done.\n";
    return 0;
}
