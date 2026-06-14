#include "Utilities.h"
#include <sstream>
#include <iomanip>

std::string ToHex(uintptr_t v)
{
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << v;
    return ss.str();
}

bool IsPrintableAscii(const std::string& s)
{
    for (unsigned char c : s)
        if (c < 0x20 || c > 0x7E) return false;
    return true;
}

bool IsPrime(size_t n)
{
    if (n < 2) return false;
    if (n < 4) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (size_t i = 5; i * i <= n; i += 6)
        if (n % i == 0 || n % (i + 2) == 0) return false;
    return true;
}
