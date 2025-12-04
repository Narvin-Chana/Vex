#pragma once

#include <cstddef>

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
        VEX_HASH_COMBINE(seed, key.vertexInputLayout);
        VEX_HASH_COMBINE(seed, key.inputAssembly);
        VEX_HASH_COMBINE(seed, key.rasterizerState);
        VEX_HASH_COMBINE(seed, key.depthStencilState);
        VEX_HASH_COMBINE(seed, key.colorBlendState);
        VEX_HASH_COMBINE(seed, key.renderTargetState);
        return seed;
    });

    DX12GraphicsPipelineState(const ComPtr<DX12Device>& device, const Key& key);

    virtual void Compile(const Shader& vertexShader,
                         const Shader& pixelShader,
                         RHIResourceLayout& resourceLayout) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;

    // Verifies that the key does not contain fields with non-default values for features which DX12 does not support.
    // Clears the unused fields which allows for changes to these fields to not impact the hash of the structure.
    static void ClearUnsupportedKeyFields(Key& key);

    ComPtr<ID3D12PipelineState> graphicsPSO;

private:
    ComPtr<DX12Device> device;
};

class DX12ComputePipelineState final : public RHIComputePipelineStateInterface
{
public:
    DX12ComputePipelineState(const ComPtr<DX12Device>& device, const Key& key);

    virtual void Compile(const Shader& computeShader, RHIResourceLayout& resourceLayout) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;

    ComPtr<ID3D12PipelineState> computePSO;

private:
    ComPtr<DX12Device> device;
};

class DX12RayTracingPipelineState final : public RHIRayTracingPipelineStateInterface
{
public:
    DX12RayTracingPipelineState(const ComPtr<DX12Device>& device, const Key& key);

    virtual void Compile(const RayTracingShaderCollection& shaderCollection,
                         RHIResourceLayout& resourceLayout,
                         ResourceCleanup& resourceCleanup,
                         RHIAllocator& allocator) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;

    void PrepareDispatchRays(D3D12_DISPATCH_RAYS_DESC& dispatchRaysDesc) const;

    ComPtr<ID3D12StateObject> stateObject;

private:
    void GenerateIdentifiers(const RayTracingShaderCollection& shaderCollection);
    void CreateShaderTables(ResourceCleanup& resourceCleanup, RHIAllocator& allocator);
    void UpdateVersions(const RayTracingShaderCollection& shaderCollection, RHIResourceLayout& resourceLayout);

    static constexpr u32 ShaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    static constexpr u32 ShaderTableAlignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

    ComPtr<DX12Device> device;

    void* rayGenerationIdentifier = nullptr;
    std::vector<void*> rayMissIdentifiers;
    std::vector<void*> hitGroupIdentifiers;
    std::vector<void*> rayCallableIdentifiers;

    MaybeUninitialized<DX12ShaderTable> rayGenerationShaderTable;
    MaybeUninitialized<DX12ShaderTable> rayMissShaderTable;
    MaybeUninitialized<DX12ShaderTable> hitGroupShaderTable;
    MaybeUninitialized<DX12ShaderTable> rayCallableShaderTable;
};

} // namespace vex::dx12