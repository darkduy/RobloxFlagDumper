#include "OutputWriter.h"
#include "Utilities.h"
#include <psapi.h>
#include <fstream>
#include <ctime>
#include <vector>
#include <iomanip>
#include <cctype>

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

    // Sanitize identifier: replace invalid C++ chars with '_'
    auto sanitize = [](const std::string& s) {
        std::string r = s;
        for (char& c : r)
            if (!isalnum((unsigned char)c) && c != '_') c = '_';
        // Can't start with digit
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
