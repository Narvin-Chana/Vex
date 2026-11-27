#pragma once

#include <utility>
#include <vector>

#include <Vex/Containers/InlineVector.h>
#include <Vex/PipelineState.h>
#include <Vex/ShaderView.h>
#include <Vex/Types.h>
#include <Vex/Utility/MaybeUninitialized.h>

#include <RHI/RHIFwd.h>

namespace vex
{

class RHIGraphicsPipelineStateBase
{
public:
    using Key = GraphicsPSOKey;

    explicit RHIGraphicsPipelineStateBase(std::string name, Key key)
        : name{ std::move(name) }
        , key{ std::move(key) }
    {
    }

    virtual void Cleanup(std::vector<RHIDeferredRelease>& deferredReleases) = 0;

    virtual void Compile(const ShaderView& vertexShader,
                         const ShaderView* pixelShader,
                         RHIResourceLayout& resourceLayout) = 0;

    virtual void Compile(const ShaderView& meshShader,
                         const ShaderView* amplificationShader,
                         const ShaderView* pixelShader,
                         RHIResourceLayout& resourceLayout) = 0;

    std::string name;
    Key key;
    u32 rootSignatureVersion = 0;
};

class RHIComputePipelineStateBase
{
public:
    using Key = ComputePSOKey;

    explicit RHIComputePipelineStateBase(std::string name, Key key)
        : name{ std::move(name) }
        , key{ std::move(key) }
    {
    }
    virtual void Compile(const ShaderView& computeShader, RHIResourceLayout& resourceLayout) = 0;
    virtual void Cleanup(std::vector<RHIDeferredRelease>& deferredReleases) = 0;

    std::string name;
    Key key;
    u32 rootSignatureVersion = 0;
};

class RHIRayTracingPipelineStateBase
{
public:
    using Key = RayTracingPSOKey;
    // One buffer per shader table type: ray generation, miss, hit group and callable.
    using ShaderTableBuffers = InlineVector<MaybeUninitialized<RHIBuffer>, 4>;

    explicit RHIRayTracingPipelineStateBase(std::string name, Key key)
        : name{ std::move(name) }
        , key{ std::move(key) }
    {
    }
    virtual ShaderTableBuffers Compile(const RayTracingShaderCollection& shaderCollection,
                                       RHIResourceLayout& resourceLayout,
                                       RHIAllocator& allocator) = 0;
    virtual void Cleanup(std::vector<RHIDeferredRelease>& deferredReleases) = 0;

    std::string name;
    Key key;
    u32 rootSignatureVersion = 0;
};

} // namespace vex
