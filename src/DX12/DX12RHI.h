#pragma once

#include <Vex/RHI.h>

#include <DX12/DX12FeatureChecker.h>

namespace vex
{
struct PlatformWindowHandle;
}

namespace vex::dx12
{

class DX12RHI : public vex::RHI
{
public:
    DX12RHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation);
    virtual ~DX12RHI() override;

    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() override;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) override;

private:
    bool enableGPUDebugLayer = false;

    ComPtr<DX12Device> device;

    ComPtr<ID3D12CommandQueue> copyQueue;
    ComPtr<ID3D12CommandQueue> asyncComputeQueue;
    ComPtr<ID3D12CommandQueue> graphicsQueue;
};

} // namespace vex::dx12