#include "DX12PhysicalDevice.h"

#include <Vex/Platform/Windows/WString.h>

#include <DX12/DX12FeatureChecker.h>

namespace vex::dx12
{

DX12PhysicalDevice::DX12PhysicalDevice(ComPtr<IDXGIAdapter4>&& adapter, const ComPtr<ID3D12Device>& device)
    : adapter(std::move(adapter))
{
    DXGI_ADAPTER_DESC3 desc;
    this->adapter->GetDesc3(&desc);

    deviceName = WStringToString(desc.Description);
    dedicatedVideoMemoryMB = static_cast<double>(desc.DedicatedVideoMemory) / (1024.0 * 1024.0);
    featureChecker = MakeUnique<DX12FeatureChecker>(device);
}

} // namespace vex::dx12
