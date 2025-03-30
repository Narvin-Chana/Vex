#include "DXGIFactory.h"

#include <Vex/Platform/Windows/WString.h>

#include <DX12/HRChecker.h>

namespace vex::dx12
{

ComPtr<IDXGIFactory7> DXGIFactory::dxgiFactory;

void DXGIFactory::InitializeDXGIFactory()
{
    chk << CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));
}

ComPtr<DX12Device> DXGIFactory::CreateDevice(IDXGIAdapter* adapter, D3D_FEATURE_LEVEL minimumFeatureLevel)
{
    ComPtr<DX12Device> device;
    chkSoft << D3D12CreateDevice(adapter, minimumFeatureLevel, IID_PPV_ARGS(&device));
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
