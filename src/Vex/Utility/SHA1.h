// SHA1 library comes from https://github.com/vog/sha1/blob/master/sha1.hpp

#pragma once
#include <array>
#include <string>
#include <cstdint>

class SHA1
{
public:
    SHA1();
    void update(const std::string& s);
    void update(std::istream& is);
    std::array<uint32_t, 5> final();

private:
    std::array<uint32_t, 5> digest;
    std::string buffer;
    uint64_t transforms;
};
