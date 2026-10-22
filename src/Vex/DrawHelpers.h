#pragma once

#include <Vex/GraphicsPipeline.h>
#include <Vex/ShaderView.h>

namespace vex
{

struct DrawDesc
{
    ShaderView vertexShader;
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
    std::optional<ShaderView> pixelShader;
    InputAssembly inputAssembly;
    RasterizerState rasterizerState;
    DepthStencilState depthStencilState;
    ColorBlendState colorBlendState;
};

} // namespace vex