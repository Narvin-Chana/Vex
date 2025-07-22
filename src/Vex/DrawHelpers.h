#pragma once

#include <optional>
#include <span>

#include <Vex/Bindings.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/ShaderKey.h>

namespace vex
{

struct DrawDescription
{
    ShaderKey vertexShader;
    ShaderKey pixelShader;
    VertexInputLayout vertexInputLayout;
    InputAssembly inputAssembly;
    RasterizerState rasterizerState;
    DepthStencilState depthStencilState;
    ColorBlendState colorBlendState;
};

struct DrawResources
{
    // Local constants bound to the root/push constants of your pass.
    std::span<const ConstantBinding> constants;
    // Read-only resources.
    std::span<const ResourceBinding> readResources;
    // Read / Write resources.
    std::span<const ResourceBinding> unorderedAccessResources;
    // Write-only resources.
    std::span<const ResourceBinding> renderTargets;
    // Write-only resource (optional).
    std::optional<ResourceBinding> depthStencil = std::nullopt;
};

} // namespace vex