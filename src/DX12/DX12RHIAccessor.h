#pragma once

#include <Vex/RHI/RHIAccessor.h>
#include <Vex/RHI/RHIFwd.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12DescriptorPool;
class DX12CommandPool;

class DX12RHIAccessor : public RHIAccessor
{
public:
    DX12RHIAccessor(ComPtr<DX12Device> device,
                    ComPtr<ID3D12CommandQueue> commandQueue,
                    DX12DescriptorPool* descriptorPool);
    virtual ~DX12RHIAccessor() override;

    DX12Device* GetNativeDevice();
    ID3D12DescriptorHeap* GetNativeDescriptorHeap();
    ID3D12CommandQueue* GetNativeCommandQueue();

    DX12DescriptorPool* GetDescriptorPool();

private:
    ComPtr<DX12Device> device;
    ComPtr<ID3D12CommandQueue> commandQueue;
    DX12DescriptorPool* descriptorPool;
};

} // namespace vex::dx12