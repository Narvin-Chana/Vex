#pragma once

#include <Vex/Hash.h>
#include <Vex/RHI/RHIFwd.h>
#include <Vex/ShaderKey.h>
#include <Vex/Types.h>

namespace vex
{

struct GraphicsPipelineStateKey
{
    ShaderKey vertexShader;
    ShaderKey pixelShader;
    // TODO: finish graphics pipeline state key
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

    Key key;
    u32 rootSignatureVersion = 0;
    u32 computeShaderVersion = 0;
};

} // namespace vex

// clang-format off
VEX_MAKE_HASHABLE(vex::GraphicsPipelineStateKey, 
    VEX_HASH_COMBINE(seed, obj.vertexShader);
    VEX_HASH_COMBINE(seed, obj.pixelShader);
);

VEX_MAKE_HASHABLE(vex::ComputePipelineStateKey, 
    VEX_HASH_COMBINE(seed, obj.computeShader);
);
// clang-format on