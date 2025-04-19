#pragma once

#include <Vex/Bindings.h>
#include <Vex/Resource.h>

namespace vex
{

class RHITexture;
class RHIBuffer;

struct RHITextureBinding
{
    ResourceBinding binding;
    ResourceUsage::Type usage;
    RHITexture* texture;
};

struct RHIBufferBinding
{
    ResourceBinding binding;
    ResourceUsage::Type usage;
    RHIBuffer* buffer;
};

} // namespace vex