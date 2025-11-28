#pragma once

#include <Vex/Types.h>

namespace vex
{

struct ShaderCompilerSettings
{
    // Additional directories in which to search for shaders (on top of the current working directory).
    std::vector<std::filesystem::path> shaderIncludeDirectories;

    // Determines if shaders should be compiled with debug symbols and without optimization. Defaults to true in debug
    // builds.
    bool enableShaderDebugging = !VEX_DEVELOPMENT;

    // Determines whether HLSL 202x features should be enabled (only useful when using DXC).
    bool enableHLSL202xFeatures = true;
};

} // namespace vex