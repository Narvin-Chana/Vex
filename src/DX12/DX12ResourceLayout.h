#pragma once

#include <Vex/RHI/RHIResourceLayout.h>
#include <Vex/Types.h>

#include <DX12/DX12FeatureChecker.h>
#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12ResourceLayout : public RHIResourceLayout
{
public:
    DX12ResourceLayout(ComPtr<DX12Device>& device);
    virtual ~DX12ResourceLayout() override;

    virtual bool ValidateGlobalConstant(const GlobalConstant& globalConstant) const override;
    virtual u32 GetMaxLocalConstantSize() const override;

    ComPtr<ID3D12RootSignature>& GetRootSignature();

private:
    void CompileRootSignature();

    ComPtr<DX12Device> device;
    ComPtr<ID3D12RootSignature> rootSignature;
};

} // namespace vex::dx12