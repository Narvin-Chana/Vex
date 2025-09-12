#pragma once

#include <Vex/RHIFwd.h>
#include <Vex/RHIImpl/RHIDescriptorSet.h>

namespace vex
{

enum class DescriptorType : u8
{
    Sampler,
    Texture,
    RWTexture,
    Buffer,
    RWBuffer,
};

class RHIDescriptorPoolBase
{
public:
    virtual RHIBindlessDescriptorSet CreateBindlessSet() = 0;
};

} // namespace vex
