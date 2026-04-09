#pragma once

#include <filesystem>
#include <vector>

#include <RHI/RHIPhysicalDevice.h>

namespace vex::sc
{

enum class CompilationTarget : u8
{
    DXIL,
    SPIRV,
};

enum class SpirvVersion : u8
{
    spirv_1_0,
    spirv_1_1,
    spirv_1_2,
    spirv_1_3,
    spirv_1_4,
    spirv_1_5,
    spirv_1_6
};

struct ShaderCompilerSettings
{
    // Determines whether to compile to the dxil or spirv target.
    CompilationTarget target = VEX_DX12 ? CompilationTarget::DXIL : CompilationTarget::SPIRV;

    // Determines the shader model to use, uses the minimum required Vex shader model by default.
    ShaderModel shaderModel = ShaderModel::SM_6_7;

    // When compiling to spirv, defines the spirv version to use, uses the minimum required Vex spirv version by default.
    SpirvVersion spirvVersion = SpirvVersion::spirv_1_6;

    // Additional directories in which to search for shaders (on top of the current working directory).
    std::vector<std::filesystem::path> shaderIncludeDirectories;

    // Determines if shaders should be compiled with optimization. Should only be disabled in specific cases.
    bool enableShaderOptimizations = true;
    // Determines if shaders should be compiled with debug symbols. Defaults to true in debug and development builds.
    bool enableShaderDebugSymbols = !VEX_SHIPPING;
    // Determines if shaders should allow for allow shader hot-reload. Defaults to true in debug and development builds.
    bool enableShaderHotReload = !VEX_SHIPPING;

    // Outputs the shader bytecode (spirv or DXIL) to a directory when a shader is compiled, warning: this will fill up
    // your drive will lots of small files if left on for too long!
    bool dumpShaderOutputBytecode = false;
};

} // namespace vex::sc