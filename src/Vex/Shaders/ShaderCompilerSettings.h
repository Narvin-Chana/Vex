#pragma once

#include <Vex/Types.h>

namespace vex
{

enum class ShaderCompilerBackend : u8
{
    DXC, // DirectX Shader Compiler
#if VEX_SLANG
    Slang,
#endif
};

struct ShaderCompilerSettings
{
    // Determines which compilation backend to use in the shader compiler.
    ShaderCompilerBackend compilerBackend = ShaderCompilerBackend::DXC;

    // Additional directories in which to search for shaders (on top of the current working directory).
    std::vector<std::filesystem::path> shaderIncludeDirectories;

    // Determines if shaders should be compiled with debug symbols.
    // Defaults to true in non-shipping builds and false in shipping.
    bool enableShaderDebugging = !VEX_SHIPPING;

    // Determines whether HLSL 202x features should be enabled.
    bool enableHLSL202xFeatures = true;
};

} // namespace vex