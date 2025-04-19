#pragma once

#include <span>
#include <vector>

#include <Vex/ShaderKey.h>
#include <Vex/Types.h>

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
    std::span<const u8> GetBlob() const
    {
        return blob;
    }

    bool IsValid() const
    {
        return !blob.empty();
    }

    bool NeedsRecompile() const
    {
        return isDirty;
    }

    void MarkDirty()
    {
        isDirty = true;
    }

    Key key;
    u32 version = 0;

private:
    bool isDirty = true;
    std::vector<u8> blob;
    std::size_t hash = 0;

    friend class ShaderCache;
};

} // namespace vex