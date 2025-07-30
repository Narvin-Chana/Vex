#pragma once

#include <span>
#include <vector>

#include <Vex/TextureSampler.h>
#include <Vex/Types.h>

namespace vex
{

class RHIResourceLayout
{
public:
    virtual ~RHIResourceLayout()
    {
    }
    virtual u32 GetMaxLocalConstantSize() const = 0;

    void SetSamplers(std::span<TextureSampler> newSamplers);
    std::span<const TextureSampler> GetStaticSamplers() const;

    u32 version = 0;

protected:
    bool isDirty = true;
    std::vector<TextureSampler> samplers;
};

} // namespace vex