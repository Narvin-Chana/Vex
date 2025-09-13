#pragma once

#include <optional>

#include <Vex/Bindings.h>
#include <Vex/Buffer.h>
#include <Vex/NonNullPtr.h>
#include <Vex/Texture.h>

#include <RHI/RHIFwd.h>

namespace vex
{

struct RHITextureBinding
{
    TextureBinding binding;
    NonNullPtr<RHITexture> texture;
};

struct RHIBufferBinding
{
    BufferBinding binding;
    NonNullPtr<RHIBuffer> buffer;
};

struct RHIDrawResources
{
    std::vector<RHITextureBinding> renderTargets;
    std::optional<RHITextureBinding> depthStencil;
};

} // namespace vex