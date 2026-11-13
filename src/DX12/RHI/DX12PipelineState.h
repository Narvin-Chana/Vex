#pragma once

#include <vector>

#include <Vex/RayTracing.h>
#include <Vex/Utility/Hash.h>
#include <Vex/Utility/MaybeUninitialized.h>

#include <RHI/RHIPipelineState.h>

#include <DX12/DX12Headers.h>
#include <DX12/DX12ShaderTable.h>

namespace vex::dx12
{

class DX12GraphicsPipelineState final : public RHIGraphicsPipelineStateBase
{
public:
    using Hasher = decltype([](const Key& key) -> std::size_t
    {
        std::size_t seed = 0;
        VEX_HASH_COMBINE(seed, key.vertexShader);
        VEX_HASH_COMBINE(seed, key.pixelShader);
        VEX_HASH_COMBINE(seed, key.meshShader);
        VEX_HASH_COMBINE(seed, key.amplificationShader);
        VEX_HASH_COMBINE(seed, key.inputAssembly);
        VEX_HASH_COMBINE(seed, key.restartIndexFormat);
        VEX_HASH_COMBINE(seed, key.rasterizerState);
        VEX_HASH_COMBINE(seed, key.depthStencilState);
        VEX_HASH_COMBINE(seed, key.colorBlendState);
        VEX_HASH_COMBINE(seed, key.renderTargetState);
        return seed;
    });

    DX12GraphicsPipelineState(const ComPtr<DX12Device>& device, std::string name, const Key& key);

    virtual void Compile(const ShaderView& vertexOrMeshShader,
                         const ShaderView* pixelShader,
                         RHIResourceLayout& resourceLayout) override;
    virtual void Compile(const ShaderView& meshShader,
                         const ShaderView* amplificationShader,
                         const ShaderView* pixelShader,
                         RHIResourceLayout& resourceLayout) override;

    virtual std::unique_ptr<RHIGraphicsPipelineState> Cleanup() override;

    ComPtr<ID3D12PipelineState> graphicsPSO;

private:
    CD3DX12_PIPELINE_STATE_STREAM6 CreatePipelineStateStream(RHIResourceLayout& resourceLayout) const;
    void CreatePipelineState(CD3DX12_PIPELINE_STATE_STREAM6& stream, RHIResourceLayout& resourceLayout);

    ComPtr<DX12Device> device;
};

class DX12ComputePipelineState final : public RHIComputePipelineStateBase
{
public:
    DX12ComputePipelineState(const ComPtr<DX12Device>& device, std::string name, const Key& key);

    virtual void Compile(const ShaderView& computeShader, RHIResourceLayout& resourceLayout) override;
    virtual std::unique_ptr<RHIComputePipelineState> Cleanup() override;

    ComPtr<ID3D12PipelineState> computePSO;

private:
    ComPtr<DX12Device> device;
};

class DX12RayTracingPipelineState final : public RHIRayTracingPipelineStateBase
{
public:
    DX12RayTracingPipelineState(const ComPtr<DX12Device>& device, std::string name, const Key& key);

    virtual std::vector<MaybeUninitialized<RHIBuffer>> Compile(const RayTracingShaderCollection& shaderCollection,
                                                               RHIResourceLayout& resourceLayout,
                                                               RHIAllocator& allocator) override;
    virtual std::unique_ptr<RHIRayTracingPipelineState> Cleanup() override;

    void PrepareDispatchRays(D3D12_DISPATCH_RAYS_DESC& dispatchRaysDesc, const TraceRaysDesc& rayTracingArgs) const;

    ComPtr<ID3D12StateObject> stateObject;

private:
    void GenerateIdentifiers(const RayTracingShaderCollection& shaderCollection);
    std::vector<MaybeUninitialized<RHIBuffer>> CreateShaderTables(RHIAllocator& allocator);

    ComPtr<DX12Device> device;

    std::vector<void*> rayGenerationIdentifiers;
    std::vector<void*> rayMissIdentifiers;
    std::vector<void*> hitGroupIdentifiers;
    std::vector<void*> rayCallableIdentifiers;

    MaybeUninitialized<DX12ShaderTable> rayGenerationShaderTable;
    MaybeUninitialized<DX12ShaderTable> rayMissShaderTable;
    MaybeUninitialized<DX12ShaderTable> hitGroupShaderTable;
    MaybeUninitialized<DX12ShaderTable> rayCallableShaderTable;
};

} // namespace vex::dx12