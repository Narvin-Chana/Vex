#pragma once

#include <filesystem>
#include <string>
#include <tuple>
#include <vector>

#include <Vex/Hash.h>

namespace vex
{

enum class ShaderType
{
    VertexShader,
    PixelShader,
    ComputeShader,
    // ...
};

struct ShaderDefine
{
    std::string name;
    std::string value;
    bool operator==(const ShaderDefine& other) const = default;
};

struct ShaderKey
{
    std::filesystem::path path;
    std::string entryPoint;
    ShaderType type;
    std::vector<ShaderDefine> defines;
    bool operator==(const ShaderKey& other) const = default;
};

} // namespace vex

// clang-format off
VEX_MAKE_HASHABLE(vex::ShaderKey, 
    VEX_HASH_COMBINE(seed, obj.path);
    VEX_HASH_COMBINE(seed, obj.entryPoint);
    VEX_HASH_COMBINE_ENUM(seed, obj.type);
    VEX_HASH_COMBINE_CONTAINER(seed, obj.defines,
        VEX_HASH_COMBINE(seed, item.name);
        VEX_HASH_COMBINE(seed, item.value);
    );
);
// clang-format on