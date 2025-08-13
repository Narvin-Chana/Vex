#pragma once

#include <Vex/Bindings.h>
#include <Vex/Buffer.h>
#include <Vex/RHIFwd.h>
#include <Vex/Texture.h>

namespace vex
{

struct RHITextureBinding
{
    TextureBinding binding;
    RHITexture* texture;
};

struct RHIBufferBinding
{
    BufferBinding binding;
    RHIBuffer* buffer;
};

struct RHIDrawResources
{
    std::vector<RHITextureBinding> renderTargets;
    std::optional<RHITextureBinding> depthStencil;
};

} // namespace vex