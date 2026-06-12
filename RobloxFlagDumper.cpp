// RobloxFlagDumper.cpp
// Fully dynamic – tự thích nghi sau mỗi lần Roblox update (educational only)
// Compile: g++ -O2 -static RobloxFlagDumper.cpp -o RobloxFlagDumper.exe
// Run as Administrator

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

#pragma comment(lib, "psapi.lib")

// =========================== HELPER CLASSES ===========================
class MemoryReader {
    HANDLE hProcess;
public:
    MemoryReader(HANDLE hp) : hProcess(hp) {}
    
    bool Read(uintptr_t addr, void* buf, size_t size) {
        SIZE_T read;
        return ReadProcessMemory(hProcess, (LPCVOID)addr, buf, size, &read) && read == size;
    }
    
    template<typename T>
    T Read(uintptr_t addr) {
        T val = {0};
        Read(addr, &val, sizeof(val));
        return val;
    }
    
    std::string ReadString(uintptr_t addr, size_t maxLen = 256) {
        char buf[maxLen+1];
        if (Read(addr, buf, maxLen)) {
            buf[maxLen] = 0;
            return std::string(buf);
        }
        return "";
    }
};

// Simple instruction decoder – chỉ xử lý RIP-relative addressing
class InstructionDecoder {
public:
    // Kiểm tra lệnh "mov rcx, [rip+disp32]" (48 8B 0D ? ? ? ?)
    static bool IsMovRcxRip(const uint8_t* bytes) {
        return bytes[0] == 0x48 && bytes[1] == 0x8B && bytes[2] == 0x0D;
    }
    
    // Lấy disp32 từ lệnh trên (offset 3 bytes)
    static int32_t GetDisp32(const uint8_t* bytes) {
        return *(int32_t*)(bytes + 3);
    }
    
    // Tính địa chỉ đích: instrAddr + 7 + disp32
    static uintptr_t ResolveRip(uintptr_t instrAddr, const uint8_t* bytes) {
        return instrAddr + 7 + GetDisp32(bytes);
    }
};

// =========================== PHÁT HIỆN CẤU TRÚC UNORDERED_MAP ===========================
// Heuristic: tìm một vùng nhớ có các field hợp lý của MSVC unordered_map
struct MapCandidate {
    uintptr_t address;      // địa chỉ của object map
    size_t bucketCount;     // bucket_count (thường là prime number)
    size_t elementCount;    // size
    uintptr_t bucketArray;  // _Buckets
    uintptr_t firstNode;    // _Head
    float score;
};

std::vector<MapCandidate> FindMapCandidates(MemoryReader& mem, uintptr_t start, uintptr_t end) {
    std::vector<MapCandidate> candidates;
    const size_t step = 0x1000;
    uint8_t buf[0x1000];
    
    for (uintptr_t addr = start; addr < end; addr += step) {
        if (!mem.Read(addr, buf, sizeof(buf))) continue;
        
        for (size_t offset = 0; offset <= sizeof(buf) - 0x30; offset += 8) {
            uintptr_t objAddr = addr + offset;
            size_t bucketCount = mem.Read<size_t>(objAddr);
            size_t elementCount = mem.Read<size_t>(objAddr + 0x18);
            
            // heuristic: bucketCount thường là số nguyên tố (hoặc 0,1,2...), elementCount <= 200000
            if (elementCount > 0 && elementCount < 200000 && bucketCount > 0 && bucketCount < 1000000) {
                uintptr_t bucketArray = mem.Read<uintptr_t>(objAddr + 0x10);
                uintptr_t firstNode = mem.Read<uintptr_t>(objAddr + 0x08);
                // bucketArray phải là địa chỉ hợp lệ, firstNode có thể null (map rỗng)
                if (bucketArray > 0x10000 && bucketArray < 0x7FFFFFFF0000) {
                    float score = 0.0f;
                    if (elementCount > 1000) score += 10.0f;
                    if (bucketCount > elementCount) score += 5.0f;
                    if (firstNode != 0) score += 5.0f;
                    candidates.push_back({objAddr, bucketCount, elementCount, bucketArray, firstNode, score});
                }
            }
        }
    }
    
    // Sắp xếp theo score giảm dần
    std::sort(candidates.begin(), candidates.end(), [](const MapCandidate& a, const MapCandidate& b) {
        return a.score > b.score;
    });
    return candidates;
}

// Duyệt unordered_map (MSVC layout cố định)
bool TraverseUnorderedMap(MemoryReader& mem, uintptr_t mapAddr, std::map<std::string, std::string>& outFlags) {
    // Đọc các field
    size_t bucketCount = mem.Read<size_t>(mapAddr);
    size_t elementCount = mem.Read<size_t>(mapAddr + 0x18);
    uintptr_t head = mem.Read<uintptr_t>(mapAddr + 0x08);  // _Head
    
    if (elementCount == 0 || elementCount > 100000) return false;
    
    uintptr_t cur = head;
    int count = 0;
    while (cur != 0 && count < elementCount) {
        // Node layout: 
        // +0x00: next
        // +0x08: hash
        // +0x10: key (std::string: ptr, size, capacity)
        // +0x28: value (bool, int, or string)
        uintptr_t next = mem.Read<uintptr_t>(cur);
        size_t hash = mem.Read<size_t>(cur + 0x08);
        uintptr_t keyPtr = mem.Read<uintptr_t>(cur + 0x10);
        size_t keySize = mem.Read<size_t>(cur + 0x18);
        if (keyPtr && keySize < 256 && keySize > 0) {
            std::string key = mem.ReadString(keyPtr, keySize);
            // Thử đọc value dạng bool (1 byte)
            uint8_t boolVal = mem.Read<uint8_t>(cur + 0x28);
            outFlags[key] = boolVal ? "true" : "false";
        }
        cur = next;
        count++;
    }
    return count > 0;
}

// =========================== TÌM GLOBAL FLAG MAP ===========================
// Chiến lược: tìm tất cả các string "FFlag*" trong .rdata, sau đó tìm các lệnh tham chiếu đến chúng
// và từ đó suy ra hàm getter và biến toàn cục.
struct FlagInfo {
    uintptr_t stringAddr;   // địa chỉ của string literal "FFlag..."
    std::string name;
};

std::vector<FlagInfo> FindFlagStrings(MemoryReader& mem, uintptr_t baseAddr, size_t imageSize) {
    std::vector<FlagInfo> result;
    const size_t scanStep = 0x1000;
    uint8_t buf[0x1000];
    
    for (uintptr_t addr = baseAddr; addr < baseAddr + imageSize; addr += scanStep) {
        if (!mem.Read(addr, buf, sizeof(buf))) continue;
        
        for (size_t i = 0; i <= sizeof(buf) - 8; ++i) {
            if (buf[i] == 'F' && buf[i+1] == 'F' && buf[i+2] == 'l' && buf[i+3] == 'a' && buf[i+4] == 'g') {
                // Tìm độ dài string (kết thúc bằng null)
                size_t len = 0;
                while (i+len < sizeof(buf) && buf[i+len] != 0) len++;
                if (len >= 5 && len < 64) {
                    std::string s((char*)&buf[i], len);
                    if (s.find("FFlag") == 0) {
                        result.push_back({addr + i, s});
                        i += len;
                    }
                }
            }
        }
    }
    return result;
}

// Quét memory để tìm các lệnh "lea rdx, [stringAddr]" (48 8D 15 ? ? ? ?) và resolve RIP
std::vector<uintptr_t> FindInstructionsReferencingString(MemoryReader& mem, uintptr_t stringAddr, uintptr_t start, uintptr_t end) {
    std::vector<uintptr_t> instrAddrs;
    uint8_t buf[4096];
    const uint8_t pattern[] = {0x48, 0x8D, 0x15, 0x00, 0x00, 0x00, 0x00}; // lea rdx, [rip+disp]
    
    for (uintptr_t addr = start; addr < end; addr += sizeof(buf) - 7) {
        if (!mem.Read(addr, buf, sizeof(buf))) continue;
        
        for (size_t i = 0; i <= sizeof(buf) - 7; ++i) {
            if (memcmp(buf+i, pattern, 3) == 0) {
                uintptr_t instrAddr = addr + i;
                int32_t disp = *(int32_t*)(buf+i+3);
                uintptr_t target = instrAddr + 7 + disp;
                if (target == stringAddr) {
                    instrAddrs.push_back(instrAddr);
                }
            }
        }
    }
    return instrAddrs;
}

// Tìm tất cả các hàm tham chiếu đến string, từ đó tìm ra getter function (hàm có nhiều tham chiếu đến các flag strings)
uintptr_t FindGetterFunction(MemoryReader& mem, const std::vector<FlagInfo>& flags, uintptr_t codeStart, uintptr_t codeEnd) {
    std::map<uintptr_t, int> functionHits;
    
    for (const auto& flag : flags) {
        auto refs = FindInstructionsReferencingString(mem, flag.stringAddr, codeStart, codeEnd);
        for (uintptr_t instr : refs) {
            // Tìm đầu hàm (scan ngược để tìm prologue: 40 53 48 83 EC ...)
            uintptr_t funcStart = instr;
            while (funcStart > codeStart) {
                uint8_t byte;
                if (!mem.Read(funcStart-1, &byte, 1)) break;
                if (byte == 0xCC || byte == 0xC3) break; // int3 or ret
                if (funcStart - instr > 0x200) break;
                funcStart--;
            }
            // Kiểm tra prologue thường gặp: 40 53 48 83 EC 20 (push rbx, sub rsp,20)
            uint8_t prologue[6];
            if (mem.Read(funcStart, prologue, 6)) {
                if (prologue[0] == 0x40 && prologue[1] == 0x53 && prologue[2] == 0x48 && prologue[3] == 0x83 && prologue[4] == 0xEC) {
                    functionHits[funcStart]++;
                }
            }
        }
    }
    
    // Hàm nào được gọi nhiều nhất có thể là getter/setter
    uintptr_t best = 0;
    int maxHits = 0;
    for (auto& p : functionHits) {
        if (p.second > maxHits) {
            maxHits = p.second;
            best = p.first;
        }
    }
    return best;
}

// Tìm biến toàn cục (FFlagList) – nó là con trỏ đến map object.
// Cách: tìm lệnh "mov rcx, [rip+offset]" mà địa chỉ đích trỏ đến một MapCandidate phù hợp.
uintptr_t FindGlobalMapPointer(MemoryReader& mem, const std::vector<MapCandidate>& maps, uintptr_t start, uintptr_t end) {
    uint8_t buf[4096];
    for (uintptr_t addr = start; addr < end; addr += sizeof(buf)-7) {
        if (!mem.Read(addr, buf, sizeof(buf))) continue;
        for (size_t i = 0; i <= sizeof(buf)-7; ++i) {
            if (InstructionDecoder::IsMovRcxRip(buf+i)) {
                uintptr_t instrAddr = addr + i;
                uintptr_t target = InstructionDecoder::ResolveRip(instrAddr, buf+i);
                // Kiểm tra xem target có trỏ đến một map candidate không
                for (const auto& map : maps) {
                    if (target == map.address) {
                        // Địa chỉ của biến con trỏ chính là instrAddr+7? Không, ta cần địa chỉ của ô nhớ chứa con trỏ.
                        // Trong lệnh mov rcx, [rip+disp], disp trỏ đến địa chỉ của con trỏ map.
                        // Vậy địa chỉ con trỏ = target
                        return target;
                    }
                }
            }
        }
    }
    return 0;
}

// =========================== MAIN ===========================
int main() {
    std::cout << "=== Roblox Fast Flag Dumper (Fully Dynamic) ===" << std::endl;
    
    // 1. Tìm process
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snapshot, &pe)) {
        do {
            if (wcscmp(pe.szExeFile, L"RobloxPlayerBeta.exe") == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &pe));
    }
    CloseHandle(snapshot);
    if (pid == 0) {
        std::cerr << "RobloxPlayerBeta.exe not found." << std::endl;
        return 1;
    }
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) {
        std::cerr << "OpenProcess failed. Run as Administrator." << std::endl;
        return 1;
    }
    
    MemoryReader mem(hProcess);
    
    // 2. Lấy base address và kích thước image
    HMODULE hMods[1024];
    DWORD cbNeeded;
    EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded);
    uintptr_t baseAddr = (uintptr_t)hMods[0];
    MODULEINFO modInfo;
    GetModuleInformation(hProcess, hMods[0], &modInfo, sizeof(modInfo));
    size_t imageSize = modInfo.SizeOfImage;
    std::cout << "Base: 0x" << std::hex << baseAddr << ", size: 0x" << imageSize << std::dec << std::endl;
    
    // 3. Tìm tất cả các string "FFlag..."
    std::cout << "Scanning for FFlag strings..." << std::endl;
    auto flagStrings = FindFlagStrings(mem, baseAddr, imageSize);
    std::cout << "Found " << flagStrings.size() << " flag strings." << std::endl;
    if (flagStrings.empty()) {
        std::cerr << "No FFlag strings found. Roblox may have changed string encoding." << std::endl;
        CloseHandle(hProcess);
        return 1;
    }
    
    // 4. Tìm hàm getter (ValueGetSet)
    uintptr_t codeStart = baseAddr;
    uintptr_t codeEnd = baseAddr + imageSize;
    uintptr_t getterFunc = FindGetterFunction(mem, flagStrings, codeStart, codeEnd);
    if (getterFunc) {
        std::cout << "Found getter function at 0x" << std::hex << getterFunc << std::dec << std::endl;
    } else {
        std::cout << "Could not locate getter function." << std::endl;
    }
    
    // 5. Tìm các ứng viên unordered_map
    std::cout << "Searching for unordered_map candidates..." << std::endl;
    auto mapCandidates = FindMapCandidates(mem, baseAddr, baseAddr + imageSize);
    std::cout << "Found " << mapCandidates.size() << " candidates." << std::endl;
    if (mapCandidates.empty()) {
        std::cerr << "No map candidates. Maybe layout changed." << std::endl;
        CloseHandle(hProcess);
        return 1;
    }
    
    // 6. Tìm con trỏ global đến map (FFlagList)
    uintptr_t fflagListPtr = FindGlobalMapPointer(mem, mapCandidates, codeStart, codeEnd);
    uintptr_t mapAddr = 0;
    if (fflagListPtr) {
        mapAddr = mem.Read<uintptr_t>(fflagListPtr);
        std::cout << "FFlagList pointer at 0x" << std::hex << fflagListPtr << " -> map at 0x" << mapAddr << std::dec << std::endl;
    } else {
        // Fallback: lấy candidate tốt nhất
        if (!mapCandidates.empty()) {
            mapAddr = mapCandidates[0].address;
            std::cout << "Using best map candidate at 0x" << std::hex << mapAddr << std::dec << std::endl;
        } else {
            std::cerr << "No map found." << std::endl;
            CloseHandle(hProcess);
            return 1;
        }
    }
    
    // 7. Duyệt map
    std::map<std::string, std::string> flags;
    if (!TraverseUnorderedMap(mem, mapAddr, flags)) {
        std::cerr << "Failed to traverse map. Offsets might be wrong." << std::endl;
        CloseHandle(hProcess);
        return 1;
    }
    std::cout << "Dumped " << flags.size() << " flags." << std::endl;
    
    // 8. Tìm FlagToValue (thường là một global pointer khác, có thể là map thứ hai)
    uintptr_t flagToValuePtr = 0;
    // Heuristic: tìm map có kích thước lớn hơn nhưng khác với map đầu
    for (auto& cand : mapCandidates) {
        if (cand.address != mapAddr && cand.elementCount > 1000) {
            flagToValuePtr = cand.address;
            break;
        }
    }
    if (flagToValuePtr == 0 && mapCandidates.size() > 1)
        flagToValuePtr = mapCandidates[1].address;
    std::cout << "FlagToValue pointer (candidate): 0x" << std::hex << flagToValuePtr << std::dec << std::endl;
    
    // 9. Xuất ra file .hpp
    std::ofstream out("RobloxFlags.hpp");
    if (!out) {
        std::cerr << "Cannot write RobloxFlags.hpp" << std::endl;
        CloseHandle(hProcess);
        return 1;
    }
    
    time_t now = time(nullptr);
    tm tm_info;
    localtime_s(&tm_info, &now);
    char timeBuf[64];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm_info);
    
    out << "// Roblox Fast Flags – Auto dumped\n";
    out << "// Total flags: " << flags.size() << "\n";
    out << "// Dumped at " << timeBuf << "\n";
    out << "#pragma once\n";
    out << "#include <cstdint>\n\n";
    
    out << "namespace FFlagOffsets\n{\n";
    out << "    uintptr_t FFlagList = 0x" << std::hex << fflagListPtr << ";\n";
    out << "    uintptr_t ValueGetSet = 0x" << std::hex << getterFunc << ";\n";
    out << "    uintptr_t FlagToValue = 0x" << std::hex << flagToValuePtr << ";\n";
    out << "}\n\n";
    
    out << "namespace FFlags\n{\n";
    int idx = 0;
    for (auto& [name, value] : flags) {
        // Sanitize name
        std::string safe = name;
        std::replace(safe.begin(), safe.end(), '-', '_');
        std::replace(safe.begin(), safe.end(), '.', '_');
        std::replace(safe.begin(), safe.end(), ' ', '_');
        out << "    // " << name << " = " << value << "\n";
        out << "    uintptr_t " << safe << " = 0x" << std::hex << (mapAddr + idx * 0x28) << ";\n";
        idx++;
    }
    out << "}\n";
    out.close();
    
    std::cout << "Exported to RobloxFlags.hpp" << std::endl;
    CloseHandle(hProcess);
    return 0;
}