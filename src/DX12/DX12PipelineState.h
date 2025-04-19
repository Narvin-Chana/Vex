#pragma once

#include <Vex/RHI/RHIPipelineState.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12GraphicsPipelineState : public RHIGraphicsPipelineState
{
public:
    DX12GraphicsPipelineState(const Key& key);
    virtual ~DX12GraphicsPipelineState() override;
    virtual void Compile(const RHIShader& vertexShader,
                         const RHIShader& pixelShader,
                         RHIResourceLayout& resourceLayout) override;

    ComPtr<ID3D12PipelineState> graphicsPSO;
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