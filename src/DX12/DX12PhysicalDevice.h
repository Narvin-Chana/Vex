#pragma once

#include <Vex/PhysicalDevice.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

struct DX12PhysicalDevice : public PhysicalDevice
{
    DX12PhysicalDevice(ComPtr<IDXGIAdapter4> adapter, const ComPtr<ID3D12Device>& device);
    ~DX12PhysicalDevice() = default;

    DX12PhysicalDevice(const DX12PhysicalDevice&) = delete;
    DX12PhysicalDevice& operator=(const DX12PhysicalDevice&) = delete;

    DX12PhysicalDevice(DX12PhysicalDevice&&) = default;
    DX12PhysicalDevice& operator=(DX12PhysicalDevice&&) = default;

    ComPtr<IDXGIAdapter4> adapter;
};

} // namespace vex::dx12