#include "DX12RHIAccessor.h"

#include <DX12/DX12CommandPool.h>
#include <DX12/DX12DescriptorPool.h>

namespace vex::dx12
{

DX12RHIAccessor::DX12RHIAccessor(ComPtr<DX12Device> device,
                                 ComPtr<ID3D12CommandQueue> commandQueue,
                                 DX12DescriptorPool* descriptorPool)
    : device(std::move(device))
    , commandQueue(std::move(commandQueue))
    , descriptorPool(descriptorPool)
{
}

DX12RHIAccessor::~DX12RHIAccessor()
{
}

DX12Device* DX12RHIAccessor::GetNativeDevice()
{
    return device.Get();
}

ID3D12DescriptorHeap* DX12RHIAccessor::GetNativeDescriptorHeap()
{
    return descriptorPool->gpuHeap.GetRawDescriptorHeap().Get();
}

ID3D12CommandQueue* DX12RHIAccessor::GetNativeCommandQueue()
{
    return commandQueue.Get();
}

DX12DescriptorPool* DX12RHIAccessor::GetDescriptorPool()
{
    return descriptorPool;
}

} // namespace vex::dx12