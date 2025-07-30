#pragma once

#include <Vex/Bindings.h>
#include <Vex/Buffer.h>
#include <Vex/RHIFwd.h>
#include <Vex/Texture.h>

namespace vex
{

struct RHITextureBinding
{
    ResourceBinding binding;
    TextureUsage::Type usage;
    RHITexture* texture;
};

struct RHIBufferBinding
{
    ResourceBinding binding;
    BufferUsage::Type usage;
    RHIBuffer* buffer;
};

} // namespace vex