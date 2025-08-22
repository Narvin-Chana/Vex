#pragma once

#include <filesystem>
#include <string>
#include <tuple>
#include <vector>

#include <magic_enum/magic_enum.hpp>

#include <Vex/Formattable.h>
#include <Vex/Hash.h>
#include <Vex/Types.h>

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
    Auto, // Will automatically deduce which compiler to use from the file extension.
    DXC,  // DirectX Shader Compiler
#if VEX_SLANG
    Slang, // Slang Compiler API
#endif
};

struct ShaderKey
{
    std::filesystem::path path;
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
    VEX_HASH_COMBINE(seed, obj.entryPoint);
    VEX_HASH_COMBINE(seed, obj.type);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.defines);
    VEX_HASH_COMBINE(seed, obj.compiler);
);

// clang-format on

VEX_FORMATTABLE(vex::ShaderDefine, "ShaderDefine(\"{}\", \"{}\")", obj.name, obj.value);

VEX_FORMATTABLE(vex::ShaderKey,
                "ShaderKey(\n\tPath: \"{}\"\n\tEntry Point: \"{}\"\n\tType: {}\n\tDefines: {}\n\tCompiler: {})",
                obj.path.string(),
                obj.entryPoint,
                std::string(magic_enum::enum_name(obj.type)),
                obj.defines,
                std::string(magic_enum::enum_name(obj.compiler)));