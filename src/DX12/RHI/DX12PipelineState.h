#pragma once

#include <cstddef>

#include <Vex/Hash.h>

#include <RHI/RHIPipelineState.h>

#include <DX12/DX12Headers.h>

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
    ~DX12GraphicsPipelineState();

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
    ~DX12ComputePipelineState();

    virtual void Compile(const Shader& computeShader, RHIResourceLayout& resourceLayout) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;

    ComPtr<ID3D12PipelineState> computePSO;

private:
    ComPtr<DX12Device> device;
};

} // namespace vex::dx12