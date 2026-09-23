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

struct RHIIndexBufferBinding
{
    IndexBufferBinding binding;
    NonNullPtr<RHIBuffer> buffer;
};

struct RHIRenderTargetBinding
{
    RenderTargetBinding binding;
    RHITexture* texture;
};

struct RHIDepthStencilBinding
{
    DepthStencilBinding binding;
    RHITexture* texture;
};

struct RHIDrawResources
{
    InlineVector<RHIRenderTargetBinding, GMaxSimultaneousRenderTargetCount> renderTargets;
    std::optional<RHIDepthStencilBinding> depthStencil = std::nullopt;
};

} // namespace vex