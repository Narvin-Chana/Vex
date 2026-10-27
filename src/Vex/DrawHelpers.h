#pragma once

#include <Vex/GraphicsPipeline.h>
#include <Vex/ShaderView.h>

namespace vex
{

struct DrawDesc
{
    ShaderView vertexShader;
    // If not provided the pixel pass is omitted. Could be used for a depth only pass
    std::optional<ShaderView> pixelShader;
    InputAssembly inputAssembly;
    RasterizerState rasterizerState;
    DepthStencilState depthStencilState;
    ColorBlendState colorBlendState;
};

struct DispatchMeshDesc
{
    std::optional<ShaderView> amplificationShader;
    ShaderView meshShader;
    // If not provided the pixel pass is omitted. Could be used for a depth only pass
    std::optional<ShaderView> pixelShader;
    InputAssembly inputAssembly;
    RasterizerState rasterizerState;
    DepthStencilState depthStencilState;
    ColorBlendState colorBlendState;
};

} // namespace vex