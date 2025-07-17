#pragma once

#include <Vex/GraphicsPipeline.h>
#include <Vex/Hash.h>
#include <Vex/RHI/RHIFwd.h>
#include <Vex/ShaderKey.h>
#include <Vex/Types.h>

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

class RHIGraphicsPipelineState
{
public:
    using Key = GraphicsPipelineStateKey;

    RHIGraphicsPipelineState(const Key& key)
        : key{ key }
    {
    }
    virtual ~RHIGraphicsPipelineState() = default;
    virtual void Compile(const RHIShader& vertexShader,
                         const RHIShader& pixelShader,
                         RHIResourceLayout& resourceLayout) = 0;
    virtual bool NeedsRecompile(const Key& newKey) = 0;
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

class RHIComputePipelineState
{
public:
    using Key = ComputePipelineStateKey;

    RHIComputePipelineState(const Key& key)
        : key{ key }
    {
    }
    virtual ~RHIComputePipelineState() = default;
    virtual void Compile(const RHIShader& computeShader, RHIResourceLayout& resourceLayout) = 0;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) = 0;

    Key key;
    u32 rootSignatureVersion = 0;
    u32 computeShaderVersion = 0;
};

} // namespace vex

// clang-format off

VEX_MAKE_HASHABLE(vex::GraphicsPipelineStateKey, 
    VEX_HASH_COMBINE(seed, obj.vertexShader);
    VEX_HASH_COMBINE(seed, obj.pixelShader);
    VEX_HASH_COMBINE(seed, obj.vertexInputLayout);
    // Not used, PSO doesn't depend on this field.
    //VEX_HASH_COMBINE(seed, obj.inputAssembly);
    VEX_HASH_COMBINE(seed, obj.rasterizerState);
    VEX_HASH_COMBINE(seed, obj.depthStencilState);
    VEX_HASH_COMBINE(seed, obj.colorBlendState);
    VEX_HASH_COMBINE(seed, obj.renderTargetState);
);

VEX_MAKE_HASHABLE(vex::ComputePipelineStateKey, 
    VEX_HASH_COMBINE(seed, obj.computeShader);
);

VEX_FORMATTABLE(vex::GraphicsPipelineStateKey, "GraphicsPipelineKey(\n\tVS: \"{}\"\n\tPS: \"{}\"\n\t",
    obj.vertexShader, obj.pixelShader
);

// clang-format on