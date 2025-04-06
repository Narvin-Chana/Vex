#include "DX12RHI.h"

#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>

#include <DX12/DX12PhysicalDevice.h>
#include <DX12/DXGIFactory.h>

namespace vex::dx12
{

DX12RHI::DX12RHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation)
{
    HMODULE d3d12Module = GetModuleHandleA("D3D12Core.dll");
    if (d3d12Module)
    {
        char path[MAX_PATH];
        GetModuleFileNameA(d3d12Module, path, MAX_PATH);
        VEX_LOG(Info,
                "Using D3D12-SDK: {0}\n\tIf this path is in the project's target directory (and not in SYSTEM32), you "
                "are correctly using the D3D12-Agility-SDK!",
                std::string(path));
    }

    DXGIFactory::InitializeDXGIFactory();
}

DX12RHI::~DX12RHI() = default;

std::vector<UniqueHandle<PhysicalDevice>> DX12RHI::EnumeratePhysicalDevices()
{
    std::vector<UniqueHandle<PhysicalDevice>> physicalDevices;

    u32 adapterIndex = 0;
    ComPtr<IDXGIAdapter4> adapter;

    while (DXGIFactory::dxgiFactory->EnumAdapterByGpuPreference(adapterIndex,
                                                                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                                IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND)
    {
        if (ComPtr<ID3D12Device> device = DXGIFactory::CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0))
        {
            physicalDevices.push_back(MakeUnique<DX12PhysicalDevice>(std::move(adapter), device));
            adapter = nullptr;
        }

        adapterIndex++;
    }

    return physicalDevices;
}

void DX12RHI::Init(const UniqueHandle<PhysicalDevice>& physicalDevice)
{
    // TODO: init device for the passed in physical device.
}

} // namespace vex::dx12