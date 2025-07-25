#pragma once

#include <string>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{
class DXGIFactory
{
public:
    static void InitializeDXGIFactory();
    // A nullptr adapter will create a device using the default adapter. Strict means this will crash the application if
    // an error occurs during creation.
    static ComPtr<DX12Device> CreateDeviceStrict(IDXGIAdapter* adapter, D3D_FEATURE_LEVEL minimumFeatureLevel);
    static std::string GetDeviceAdapterName(const ComPtr<ID3D12Device>& device);

    static ComPtr<IDXGISwapChain4> CreateSwapChain(const DXGI_SWAP_CHAIN_DESC1& desc,
                                                   const ComPtr<ID3D12CommandQueue>& commandQueue,
                                                   HWND hwnd);

    static ComPtr<IDXGIFactory7> dxgiFactory;
};

} // namespace vex::dx12