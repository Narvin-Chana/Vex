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

    // Outputs the shader bytecode (spirv or DXIL) to a directory when a shader is compiled
    bool dumpShaderOutputBytecode = false;
};

} // namespace vex