#pragma once

#include <string>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{
class DXGIFactory
{
public:
    static ComPtr<ID3D12Device> CreateDevice(IDXGIAdapter* adapter);

    static std::string GetDeviceAdapterName(const ComPtr<ID3D12Device>& device);
};

} // namespace vex::dx12