#pragma once

#include <Vex/RHI/RHIPipelineState.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12GraphicsPipelineState : public RHIGraphicsPipelineState
{
public:
    DX12GraphicsPipelineState(const ComPtr<DX12Device>& device, const Key& key);
    virtual ~DX12GraphicsPipelineState() override;
    virtual void Compile(const RHIShader& vertexShader,
                         const RHIShader& pixelShader,
                         RHIResourceLayout& resourceLayout) override;
    virtual bool NeedsRecompile(const Key& newKey) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;

    // Verifies that the key does not contain fields with non-default values for features which DX12 does not support.
    // Clears the unused fields which allows for changes to these fields to not impact the hash of the structure.
    static void ClearUnsupportedKeyFields(Key& key);

    ComPtr<ID3D12PipelineState> graphicsPSO;

private:
    ComPtr<DX12Device> device;
};

class DX12ComputePipelineState : public RHIComputePipelineState
{
public:
    DX12ComputePipelineState(const ComPtr<DX12Device>& device, const Key& key);

    virtual ~DX12ComputePipelineState() override;

    virtual void Compile(const RHIShader& computeShader, RHIResourceLayout& resourceLayout) override;
    virtual void Cleanup(ResourceCleanup& resourceCleanup) override;

    ComPtr<ID3D12PipelineState> computePSO;

private:
    ComPtr<DX12Device> device;
};

} // namespace vex::dx12