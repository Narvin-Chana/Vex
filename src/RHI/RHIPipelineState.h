#pragma once

#include <memory>
#include <utility>
#include <vector>

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

    virtual std::unique_ptr<RHIGraphicsPipelineState> Cleanup() = 0;

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
    virtual std::unique_ptr<RHIComputePipelineState> Cleanup() = 0;

    std::string name;
    Key key;
    u32 rootSignatureVersion = 0;
};

class RHIRayTracingPipelineStateBase
{
public:
    using Key = RayTracingPSOKey;

    explicit RHIRayTracingPipelineStateBase(std::string name, Key key)
        : name{ std::move(name) }
        , key{ std::move(key) }
    {
    }
    virtual std::vector<MaybeUninitialized<RHIBuffer>> Compile(const RayTracingShaderCollection& shaderCollection,
                                                               RHIResourceLayout& resourceLayout,
                                                               RHIAllocator& allocator) = 0;
    virtual std::unique_ptr<RHIRayTracingPipelineState> Cleanup() = 0;

    std::string name;
    Key key;
    u32 rootSignatureVersion = 0;
};

} // namespace vex
