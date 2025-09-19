#pragma once

#include <Vex/Types.h>

#include <RHI/RHIResourceLayout.h>

#include <DX12/DX12FeatureChecker.h>
#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12ResourceLayout final : public RHIResourceLayoutBase
{
public:
    DX12ResourceLayout(ComPtr<DX12Device>& device);
    ~DX12ResourceLayout();

    ComPtr<ID3D12RootSignature>& GetRootSignature();

    DX12ResourceLayout(DX12ResourceLayout&&) = default;
    DX12ResourceLayout& operator=(DX12ResourceLayout&&) = default;

private:
    void CompileRootSignature();

    ComPtr<DX12Device> device;
    ComPtr<ID3D12RootSignature> rootSignature;
};

} // namespace vex::dx12