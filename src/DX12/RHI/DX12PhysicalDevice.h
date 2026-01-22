#pragma once

#include <RHI/RHIPhysicalDevice.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12PhysicalDevice : public RHIPhysicalDeviceBase
{
public:
    DX12PhysicalDevice(ComPtr<IDXGIAdapter4> adapter, const ComPtr<ID3D12Device>& device);
    ~DX12PhysicalDevice() = default;

    DX12PhysicalDevice(const DX12PhysicalDevice&) = delete;
    DX12PhysicalDevice& operator=(const DX12PhysicalDevice&) = delete;

    DX12PhysicalDevice(DX12PhysicalDevice&&) = default;
    DX12PhysicalDevice& operator=(DX12PhysicalDevice&&) = default;

    ComPtr<IDXGIAdapter4> adapter;
};

} // namespace vex::dx12