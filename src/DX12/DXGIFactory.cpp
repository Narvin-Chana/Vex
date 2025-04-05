#include "DXGIFactory.h"

#include <Vex/Platform/Windows/WString.h>

#include <DX12/HRChecker.h>

namespace vex::dx12
{

ComPtr<ID3D12Device> DXGIFactory::CreateDevice(IDXGIAdapter* adapter)
{
    ComPtr<ID3D12Device10> device;
    // TODO: convert feature level from FeatureChecker::MinimumFeatureLevel to D3D12_FEATURE_LEVEL
    chk << D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
    return device;
}

std::string DXGIFactory::GetDeviceAdapterName(const ComPtr<ID3D12Device>& device)
{
    if (!device)
    {
        return "Unknown Adapter";
    }

    // First, we need to get the LUID (Locally Unique Identifier) of the adapter
    LUID adapterLuid = device->GetAdapterLuid();

    // Create a DXGI factory
    ComPtr<IDXGIFactory4> dxgiFactory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory))))
    {
        return "Error creating DXGI Factory";
    }

    // Enumerate adapters to find the one matching our LUID
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 adapterDesc;
        chk << adapter->GetDesc1(&adapterDesc);

        // Compare LUIDs
        if (adapterDesc.AdapterLuid.LowPart == adapterLuid.LowPart &&
            adapterDesc.AdapterLuid.HighPart == adapterLuid.HighPart)
        {
            return WStringToString(adapterDesc.Description);
        }
    }

    return "Adapter Not Found";
}

} // namespace vex::dx12
