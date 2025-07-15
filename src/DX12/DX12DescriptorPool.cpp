#include "DX12DescriptorPool.h"

namespace vex::dx12
{

DX12DescriptorPool::DX12DescriptorPool(ComPtr<DX12Device>& device)
    // TODO: allow resizing, currently disallowed
    : device{ device }
    , allocator(DefaultHeapSize)
    , generations(DefaultHeapSize)
    , gpuHeap(device, DefaultHeapSize)
    , nullHeap(device, 1)
{
    // Fill in the null heap with a null SRV.
    D3D12_SHADER_RESOURCE_VIEW_DESC nullDesc = {};
    nullDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    nullDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    nullDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    nullDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(nullptr, &nullDesc, GetNullDescriptor());
}

DX12DescriptorPool::~DX12DescriptorPool() = default;

BindlessHandle DX12DescriptorPool::AllocateStaticDescriptor()
{
    std::scoped_lock lock{ mutex };
    if (allocator.freeIndices.empty())
    {
        // TODO: add resizing
        // Resize(gpuHeap.size() * 2);
    }

    u32 index = allocator.Allocate();
    // A bindless handle is initially created without any descriptor in the slot.
    // We suppose that this will then be filled in the the user of the BindlessHandle.
    return BindlessHandle::CreateHandle(index, generations[index]);
}

void DX12DescriptorPool::FreeStaticDescriptor(BindlessHandle handle)
{
    std::scoped_lock lock{ mutex };
    auto idx = handle.GetIndex();
    device->CopyDescriptorsSimple(1,
                                  gpuHeap.GetCPUDescriptorHandle(idx),
                                  GetNullDescriptor(),
                                  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    generations[idx]++;
    allocator.Deallocate(idx);
}

BindlessHandle DX12DescriptorPool::AllocateDynamicDescriptor()
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
    return handle.GetGeneration() == generations[handle.GetIndex()];
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

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12DescriptorPool::GetNullDescriptor()
{
    return nullHeap.GetCPUDescriptorHandle(0);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DX12DescriptorPool::GetGPUDescriptor(BindlessHandle handle)
{
    VEX_ASSERT(IsValid(handle), "Invalid handle passed to DX12 Descriptor Pool.");
    return gpuHeap.GetGPUDescriptorHandle(handle.GetIndex());
}

} // namespace vex::dx12