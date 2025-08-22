#pragma once

#include <Vex/Types.h>

namespace vex
{

struct ShaderCompilerSettings
{
    // Additional directories in which to search for shaders (on top of the current working directory).
    std::vector<std::filesystem::path> shaderIncludeDirectories;

    // Determines if shaders should be compiled with debug symbols.
    // Defaults to true in non-shipping builds and false in shipping.
    bool enableShaderDebugging = !VEX_SHIPPING;

    // Determines whether HLSL 202x features should be enabled.
    bool enableHLSL202xFeatures = true;
};

} // namespace vex