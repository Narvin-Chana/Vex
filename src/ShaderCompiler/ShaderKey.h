#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <Vex/ShaderView.h>
#include <Vex/Types.h>
#include <Vex/Utility/Hash.h>

namespace vex::sc
{

inline bool IsRayTracingShader(ShaderType shaderType)
{
    return shaderType >= ShaderType::RayGenerationShader && shaderType <= ShaderType::RayCallableShader;
}

struct ShaderDefine
{
    std::string name;
    std::string value = "1";
    constexpr bool operator==(const ShaderDefine& other) const = default;
};

enum class ShaderCompilerBackend : u8
{
    Auto, // Will attempt to deduce which compiler to use from the ShaderKey's (virtual) filepath extension.
#if VEX_DXC
    DXC, // DirectX Shader Compiler (for HLSL)
#endif
#if VEX_SLANG
    Slang, // Slang Compiler API (for Slang)
#endif
};

struct ShaderKey
{
    // Virtual filepath, used to identify the shader.
    std::string filepath;
    // Shader entry point.
    std::string entryPoint;
    // Shader compilation type (PixelShader, ComputeShader, ...).
    ShaderType type;
    // Shader preprocessor defines.
    std::vector<ShaderDefine> defines;
    // Determines which compilation backend to use in the shader compiler.
    ShaderCompilerBackend compiler = ShaderCompilerBackend::Auto;

    constexpr bool operator==(const ShaderKey& other) const = default;
};

} // namespace vex::sc

// clang-format off

VEX_MAKE_HASHABLE(vex::sc::ShaderDefine,
    VEX_HASH_COMBINE(seed, obj.name);
    VEX_HASH_COMBINE(seed, obj.value);  
);

VEX_MAKE_HASHABLE(vex::sc::ShaderKey,
    VEX_HASH_COMBINE(seed, obj.filepath);
    VEX_HASH_COMBINE(seed, obj.entryPoint);
    VEX_HASH_COMBINE(seed, obj.type);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.defines);
    VEX_HASH_COMBINE(seed, obj.compiler);
);

VEX_FORMATTABLE(vex::sc::ShaderDefine,
    "[\"{}\": \"{}\"]",
    obj.name,
    obj.value
);

VEX_FORMATTABLE(
    vex::sc::ShaderKey,
    "ShaderKey(\n\tKey Hash: \"{}\"\n\tFilepath: \"{}\"\n\tEntry Point: \"{}\"\n\tType: {}\n\tDefines: {}\n\tCompiler: {})",
    std::hash<vex::sc::ShaderKey>{}(obj),
    obj.filepath,
    obj.entryPoint,
    std::format("{}", obj.type),
    obj.defines,
    std::format("{}", obj.compiler)
);

// clang-format on
