#include "Logger.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>

static std::ofstream& GetLogFile()
{
    static std::ofstream f("dumper.log", std::ios::trunc);
    return f;
}

static std::string GetTimestamp()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return buf;
}

static void SetConsoleColor(int code) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(h, (WORD)code);
}

static void ResetConsoleColor() { 
    SetConsoleColor(7);  // white
}

static void WriteLog(const char* level, const std::string& msg, int color)
{
    std::string stamp = GetTimestamp();
    // Console (colored)
    std::cout << "[" << stamp << "] ";
    SetConsoleColor(color);
    std::cout << level;
    ResetConsoleColor();
    std::cout << " " << msg << "\n";
    // File (plain)
    GetLogFile() << "[" << stamp << "] " << level << " " << msg << "\n";
    GetLogFile().flush();
}

void Log::Init(const std::string& title)
{
    // Enable ANSI on Windows 10+
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    GetLogFile() << "========================================\n"
                 << " " << title << "\n"
                 << " Started: " << GetTimestamp() << "\n"
                 << "========================================\n";

    SetConsoleColor(11); // cyan
    std::cout << "=== " << title << " ===\n";
    ResetConsoleColor();
    std::cout << "\n";
}

void Log::Info(const std::string& msg) { 
    WriteLog("[INFO]", msg, 10);  // green
}

void Log::Warn(const std::string& msg) { 
    WriteLog("[WARN]", msg, 14);  // yellow
}

void Log::Err(const std::string& msg) { 
    WriteLog("[ERR ]", msg, 12);  // red
}

void Log::Step(const std::string& msg)
{
    std::string line = "\n-- " + msg + " --";
    std::cout << "\n";
    SetConsoleColor(11);
    std::cout << line << "\n";
    ResetConsoleColor();
    GetLogFile() << line << "\n";
    GetLogFile().flush();
}

void Log::Progress(size_t done, size_t total, const std::string& label)
{
    if (total == 0) return;
    int pct  = (int)(done * 100 / total);
    int fill = pct / 5;                  // bar width = 20
    std::string bar(fill, '=');
    if (fill < 20) bar += '>';
    bar.resize(20, ' ');

    // Overwrite current line on console
    std::cout << "\r  [" << bar << "] " << std::setw(3) << pct
              << "% (" << done << "/" << total << " " << label << ")   " << std::flush;

    // Log to file only at 25 / 50 / 75 / 100 %
    if (pct == 25 || pct == 50 || pct == 75 || pct == 100) {
        GetLogFile() << "  [progress] " << pct << "% (" << done << "/" << total
                     << " " << label << ")\n";
        GetLogFile().flush();
    }
}

void Log::ProgressDone(const std::string& label)
{
    std::cout << "\r  [====================] 100% " << label << " done          \n";
    GetLogFile() << "  [progress] done – " << label << "\n";
    GetLogFile().flush();
}

void Log::Detail(const std::string& msg)
{
    std::cout << "    " << msg << "\n";
    GetLogFile() << "    " << msg << "\n";
    GetLogFile().flush();
}
