#pragma once

#include <string>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{
class DXGIFactory
{
public:
    static void InitializeDXGIFactory();
    // A nullptr adapter will create a device using the default adapter.
    static ComPtr<ID3D12Device> CreateDevice(IDXGIAdapter* adapter, D3D_FEATURE_LEVEL minimumFeatureLevel);
    static std::string GetDeviceAdapterName(const ComPtr<ID3D12Device>& device);

    static ComPtr<IDXGIFactory7> dxgiFactory;
};

} // namespace vex::dx12