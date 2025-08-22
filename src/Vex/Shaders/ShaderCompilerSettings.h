#pragma once

#include <Vex/Types.h>

namespace vex
{

enum class ShaderCompilerBackend : u8
{
    DXC,
};

struct ShaderCompilerSettings
{
    // Determines which compilation backend to use in the shader compiler.
    ShaderCompilerBackend compilerBackend = ShaderCompilerBackend::DXC;

    // Determines if shaders should be compiled with debug symbols.
    // Defaults to true in non-shipping builds and false in shipping.
    bool enableShaderDebugging = !VEX_SHIPPING;

    // Determines whether HLSL 202x features should be enabled.
    bool enableHLSL202xFeatures = true;
};

} // namespace vex