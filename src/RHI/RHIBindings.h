#pragma once

#include <optional>

#include <Vex/Bindings.h>
#include <Vex/Containers/InlineVector.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIFwd.h>

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
    NonNullPtr<RHIBuffer> buffer;
};

struct RHIDrawResources
{
    InlineVector<RHITextureBinding, GMaxSimultaneousRenderTargetCount> renderTargets;
    std::optional<RHITextureBinding> depthStencil;
};

} // namespace vex