#pragma once

#include <filesystem>
#include <string>
#include <tuple>
#include <vector>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Types.h>
#include <Vex/Utility/Formattable.h>
#include <Vex/Utility/Hash.h>

namespace vex
{

enum class ShaderType : u8
{
    // Graphics Pipeline Shaders
    VertexShader,
    PixelShader,
    // Compute Pipeline Shaders
    ComputeShader,
    // Ray Tracing Shaders
    RayGenerationShader,
    RayMissShader,
    RayClosestHitShader,
    RayAnyHitShader,
    RayIntersectionShader,
    RayCallableShader,
    // ... Amplification, Task, Geometry, Hull, Domain are currently not supported...
};

inline bool IsRayTracingShader(ShaderType shaderType)
{
    return shaderType >= ShaderType::RayGenerationShader && shaderType <= ShaderType::RayCallableShader;
}

struct ShaderDefine
{
    std::string name;
    std::string value = "1";
    bool operator==(const ShaderDefine& other) const = default;
};

enum class ShaderCompilerBackend : u8
{
    Auto, // Will attempt to deduce which compiler to use from the file extension, if using shader sources, this will
          // fallback to DXC.
    DXC,  // DirectX Shader Compiler (for HLSL)
#if VEX_SLANG
    Slang, // Slang Compiler API (for Slang)
#endif
};

struct ShaderKey
{
    // Vex accepts either a filepath, or the sourceCode directly.
    // If both are filled in, we prefer the filepath.

    std::filesystem::path path;
    std::string sourceCode;

    std::string entryPoint;
    ShaderType type;
    std::vector<ShaderDefine> defines;
    // Determines which compilation backend to use in the shader compiler.
    ShaderCompilerBackend compiler = ShaderCompilerBackend::Auto;
    bool operator==(const ShaderKey& other) const = default;
};

} // namespace vex

// clang-format off

VEX_MAKE_HASHABLE(vex::ShaderDefine,
    VEX_HASH_COMBINE(seed, obj.name);
    VEX_HASH_COMBINE(seed, obj.value);  
);

VEX_MAKE_HASHABLE(vex::ShaderKey, 
    VEX_HASH_COMBINE(seed, obj.path);
    VEX_HASH_COMBINE(seed, obj.sourceCode);
    VEX_HASH_COMBINE(seed, obj.entryPoint);
    VEX_HASH_COMBINE(seed, obj.type);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.defines);
    VEX_HASH_COMBINE(seed, obj.compiler);
);

// clang-format on

VEX_FORMATTABLE(vex::ShaderDefine, "ShaderDefine(\"{}\", \"{}\")", obj.name, obj.value);

VEX_FORMATTABLE(
    vex::ShaderKey,
    "ShaderKey(\n\tKey Hash: \"{}\"\n\t{}: \"{}\"\n\tEntry Point: \"{}\"\n\tType: {}\n\tDefines: {}\n\tCompiler: {})",
    std::hash<vex::ShaderKey>{}(obj),
    !obj.path.empty() ? "Path" : "Source code",
    !obj.path.empty()
        ? obj.path.string()
        : (obj.sourceCode.size() <= 500
               ? obj.sourceCode
               : obj.sourceCode.substr(0, 500).append("\"\n\t... rest is cutoff due to shader source being too long!")),
    obj.entryPoint,
    std::string(magic_enum::enum_name(obj.type)),
    obj.defines,
    std::string(magic_enum::enum_name(obj.compiler)));