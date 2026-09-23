#pragma once

#include <RHI/RHIResourceLayout.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12ResourceLayout final : public RHIResourceLayoutBase
{
public:
    explicit DX12ResourceLayout(ComPtr<DX12Device>& device);
    ~DX12ResourceLayout();

    DX12ResourceLayout(DX12ResourceLayout&&) = default;
    DX12ResourceLayout& operator=(DX12ResourceLayout&&) = default;

    ComPtr<ID3D12RootSignature>& GetRootSignature();

private:
    void CompileRootSignature();

    ComPtr<DX12Device> device;
    ComPtr<ID3D12RootSignature> rootSignature;
};

} // namespace vex::dx12