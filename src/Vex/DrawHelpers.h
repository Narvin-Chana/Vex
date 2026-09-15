#pragma once

#include <Vex/GraphicsPipeline.h>
#include <Vex/ShaderView.h>

namespace vex
{

struct DrawDesc
{
    ShaderView vertexShader;
    ShaderView pixelShader;
    InputAssembly inputAssembly;
    RasterizerState rasterizerState;
    DepthStencilState depthStencilState;
    ColorBlendState colorBlendState;
};

} // namespace vex