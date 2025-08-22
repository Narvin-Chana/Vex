#pragma once

#include <optional>
#include <span>

#include <Vex/Bindings.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Shaders/ShaderKey.h>

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

} // namespace vex