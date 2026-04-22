#pragma once

#include <string_view>

#include <Vex/Containers/Span.h>
#include <Vex/Types.h>
#include <Vex/Utility/Hash.h>
#include <Vex/Utility/NonNullPtr.h>
#include <Vex/Utility/Formattable.h>

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

struct ShaderView
{
    constexpr ShaderView() = default;
    constexpr ShaderView(const std::string_view name,
                         const std::string_view entryPoint,
                         const Span<const byte> bytecode,
                         const SHA1HashDigest& hash,
                         const ShaderType type)
        : name(name)
        , entryPoint(entryPoint)
        , bytecode(bytecode)
        , hash(hash)
        , type(type)
    {
    }

    [[nodiscard]] bool IsValid() const
    {
        return !bytecode.empty();
    }

    bool operator==(const ShaderView& other) const
    {
        return name == other.name && entryPoint == other.entryPoint && hash == other.hash && type == other.type;
    }

    // Errored shaders are silently ignored (Vex will not error out when compared to invalid shaders).
    static constexpr ShaderView CreateErroredShader(ShaderView&& view)
    {
        view.isErrored = true;
        return view;
    }

    std::string_view name;
    std::string_view entryPoint;
    Span<const byte> bytecode;
    SHA1HashDigest hash;
    ShaderType type;

private:
    [[nodiscard]] bool IsErrored() const
    {
        return isErrored;
    }

    bool isErrored = false;

    friend class PipelineStateCache;
};

} // namespace vex

// clang-format off

VEX_MAKE_HASHABLE(vex::ShaderView,
    VEX_HASH_COMBINE(seed, obj.hash);
);

// clang-format on
