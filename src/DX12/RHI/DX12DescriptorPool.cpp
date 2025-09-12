#include "DX12DescriptorPool.h"

namespace vex::dx12
{

DX12DescriptorPool::DX12DescriptorPool(ComPtr<DX12Device> device)
    : device{ device }
{
}

RHIBindlessDescriptorSet DX12DescriptorPool::CreateBindlessSet()
{
    return DX12BindlessDescriptorSet(device);
}

} // namespace vex::dx12