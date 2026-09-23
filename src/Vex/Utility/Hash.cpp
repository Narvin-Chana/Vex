#include "Hash.h"

#include <format>

std::string vex::HashToString(const SHA1HashDigest& hash)
{
    std::string result;
    result.reserve(hash.value.size() * 8);
    for (u32 word : hash.value)
    {
        std::format_to(std::back_inserter(result), "{:08x}", word);
    }
    return result;
}