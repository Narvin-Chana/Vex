#pragma once

#include <Vex/GraphicsPipeline.h>
#include <Vex/Shaders/RayTracingShaders.h>
#include <Vex/Shaders/Shader.h>
#include <Vex/Shaders/ShaderKey.h>
#include <Vex/Types.h>
#include <Vex/Utility/Hash.h>

#include <RHI/RHIFwd.h>

namespace vex
{

class ResourceCleanup;

struct GraphicsPipelineStateKey
{
    ShaderKey vertexShader;
    ShaderKey pixelShader;
    VertexInputLayout vertexInputLayout;
    InputAssembly inputAssembly;
    RasterizerState rasterizerState;
    DepthStencilState depthStencilState;
    ColorBlendState colorBlendState;
    RenderTargetState renderTargetState;

    bool operator==(const GraphicsPipelineStateKey& other) const = default;
};

class RHIGraphicsPipelineStateBase
{
public:
    using Key = GraphicsPipelineStateKey;

    RHIGraphicsPipelineStateBase(const Key& key)
        : key{ key }
    {
    }
    virtual void Compile(const Shader& vertexShader, const Shader& pixelShader, RHIResourceLayout& resourceLayout) = 0;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) = 0;

    Key key;
    u32 rootSignatureVersion = 0;
    u32 vertexShaderVersion = 0;
    u32 pixelShaderVersion = 0;
};

struct ComputePipelineStateKey
{
    ShaderKey computeShader;
    bool operator==(const ComputePipelineStateKey& other) const = default;
};

class RHIComputePipelineStateInterface
{
public:
    using Key = ComputePipelineStateKey;

    RHIComputePipelineStateInterface(const Key& key)
        : key{ key }
    {
    }
    virtual void Compile(const Shader& computeShader, RHIResourceLayout& resourceLayout) = 0;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) = 0;

    Key key;
    u32 rootSignatureVersion = 0;
    u32 computeShaderVersion = 0;
};

using RayTracingPipelineStateKey = RayTracingPassDesc;

class RHIRayTracingPipelineStateInterface
{
public:
    using Key = RayTracingPipelineStateKey;

    RHIRayTracingPipelineStateInterface(const Key& key)
        : key{ key }
    {
    }
    virtual void Compile(const RayTracingShaderCollection& shaderCollection,
                         RHIResourceLayout& resourceLayout,
                         ResourceCleanup& resourceCleanup,
                         RHIAllocator& allocator) = 0;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) = 0;

    Key key;
    u32 rootSignatureVersion = 0;
    u32 rayGenerationShaderVersion = 0;
    std::vector<u32> rayMissShaderVersions;
    struct HitGroupVersions
    {
        u32 rayClosestHitVersion = 0;
        std::optional<u32> rayAnyHitVersion;
        std::optional<u32> rayIntersectionVersion;
        std::optional<u32> rayCallableVersion;
    };
    std::vector<HitGroupVersions> hitGroupVersions;
    std::vector<u32> rayCallableShaderVersions;
};

} // namespace vex

// clang-format off

VEX_MAKE_HASHABLE(vex::ComputePipelineStateKey, 
    VEX_HASH_COMBINE(seed, obj.computeShader);
);


VEX_FORMATTABLE(vex::GraphicsPipelineStateKey, "GraphicsPipelineKey(\n\tVS: \"{}\"\n\tPS: \"{}\"\n\t",
    obj.vertexShader, obj.pixelShader
);

VEX_FORMATTABLE(vex::ComputePipelineStateKey, "ComputePipelineKey(\n\tCS: \"{}\"\n\t",
    obj.computeShader
);

// clang-format on