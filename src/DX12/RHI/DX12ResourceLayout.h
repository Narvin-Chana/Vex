#pragma once

#include <Vex/Types.h>

#include <RHI/RHIResourceLayout.h>

#include <DX12/DX12FeatureChecker.h>
#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12ResourceLayout final : public RHIResourceLayoutInterface
{
public:
    DX12ResourceLayout(ComPtr<DX12Device>& device);
    ~DX12ResourceLayout();

    virtual u32 GetMaxLocalConstantSize() const override;

    ComPtr<ID3D12RootSignature>& GetRootSignature();

private:
    void CompileRootSignature();

    ComPtr<DX12Device> device;
    ComPtr<ID3D12RootSignature> rootSignature;
};

} // namespace vex::dx12