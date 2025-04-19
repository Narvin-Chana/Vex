#pragma once

#include <span>

#include <Vex/ShaderKey.h>
#include <Vex/Types.h>

class IDxcBlob;

namespace vex
{

class RHIShader
{
public:
    using Key = ShaderKey;

    RHIShader(const Key& key)
        : key{ key }
    {
    }
    virtual ~RHIShader() = default;
    virtual std::span<const u8> GetBlob() const = 0;

    Key key;
    u32 version = 0;
};

} // namespace vex