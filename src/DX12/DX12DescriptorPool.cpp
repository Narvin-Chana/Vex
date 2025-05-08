#include "DX12DescriptorPool.h"

namespace vex::dx12
{

DX12DescriptorPool::DX12DescriptorPool(ComPtr<DX12Device>& device)
    // TODO: allow resizing, currently disallowed
    : allocator(DefaultHeapSize)
    , gpuHeap(device, DefaultHeapSize)
{
}

DX12DescriptorPool::~DX12DescriptorPool() = default;

BindlessHandle DX12DescriptorPool::AllocateStaticDescriptor(const RHITexture& texture)
{
    std::scoped_lock lock{ mutex };
    VEX_NOT_YET_IMPLEMENTED();
    return BindlessHandle();
}

BindlessHandle DX12DescriptorPool::AllocateStaticDescriptor(const RHIBuffer& buffer)
{
    std::scoped_lock lock{ mutex };
    VEX_NOT_YET_IMPLEMENTED();
    return BindlessHandle();
}

void DX12DescriptorPool::FreeStaticDescriptor(BindlessHandle handle)
{
    std::scoped_lock lock{ mutex };
    VEX_NOT_YET_IMPLEMENTED();
}

BindlessHandle DX12DescriptorPool::AllocateDynamicDescriptor(const RHITexture& texture)
{
    std::scoped_lock lock{ mutex };
    VEX_NOT_YET_IMPLEMENTED();
    return BindlessHandle();
}

BindlessHandle DX12DescriptorPool::AllocateDynamicDescriptor(const RHIBuffer& buffer)
{
    std::scoped_lock lock{ mutex };
    VEX_NOT_YET_IMPLEMENTED();
    return BindlessHandle();
}

void DX12DescriptorPool::FreeDynamicDescriptor(BindlessHandle handle)
{
    std::scoped_lock lock{ mutex };
    VEX_NOT_YET_IMPLEMENTED();
}

bool DX12DescriptorPool::IsValid(BindlessHandle handle)
{
    std::scoped_lock lock{ mutex };
    VEX_NOT_YET_IMPLEMENTED();
    return true;
}

void DX12DescriptorPool::CopyDescriptor(ComPtr<DX12Device>& device,
                                        BindlessHandle handle,
                                        CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor)
{
    VEX_ASSERT(IsValid(handle), "Invalid handle passed to DX12 Descriptor Pool.");
    device->CopyDescriptorsSimple(1, GetCPUDescriptor(handle), descriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12DescriptorPool::GetCPUDescriptor(BindlessHandle handle)
{
    VEX_ASSERT(IsValid(handle), "Invalid handle passed to DX12 Descriptor Pool.");
    return gpuHeap.GetCPUDescriptorHandle(handle.GetIndex());
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DX12DescriptorPool::GetGPUDescriptor(BindlessHandle handle)
{
    VEX_ASSERT(IsValid(handle), "Invalid handle passed to DX12 Descriptor Pool.");
    return gpuHeap.GetGPUDescriptorHandle(handle.GetIndex());
}

} // namespace vex::dx12