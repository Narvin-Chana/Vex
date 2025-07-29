#pragma once

#include <mutex>

#include <Vex/Containers/FreeList.h>
#include <Vex/Handle.h>
#include <Vex/RHI/RHIDescriptorPool.h>

#include <DX12/DX12DescriptorHeap.h>
#include <DX12/DX12Headers.h>

namespace vex::dx12
{

struct BindlessHandle : Handle<BindlessHandle>
{
};

static constexpr BindlessHandle GInvalidBindlessHandle;

class DX12DescriptorPool : public RHIDescriptorPool
{
public:
    DX12DescriptorPool(ComPtr<DX12Device>& device);
    virtual ~DX12DescriptorPool() override;

    BindlessHandle AllocateStaticDescriptor();
    BindlessHandle AllocateStaticDescriptor(const RHIBuffer& buffer);
    void FreeStaticDescriptor(BindlessHandle handle);

    BindlessHandle AllocateDynamicDescriptor(const RHITexture& texture);
    BindlessHandle AllocateDynamicDescriptor(const RHIBuffer& buffer);
    void FreeDynamicDescriptor(BindlessHandle handle);

    bool IsValid(BindlessHandle handle);

    void CopyDescriptor(ComPtr<DX12Device>& device, BindlessHandle handle, CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor);
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptor(BindlessHandle handle);
    CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptor(BindlessHandle handle);

private:
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetNullDescriptor();

    ComPtr<DX12Device> device;

    // Shared by all threads. There might be a better way to do this (especially for when you create many textures in a
    // row).
    std::mutex mutex;
    FreeListAllocator allocator;
    std::vector<u8> generations;
    DX12DescriptorHeap<HeapType::CBV_SRV_UAV, HeapFlags::SHADER_VISIBLE> gpuHeap;

    // Used to store a null descriptor, useful for avoiding invalid texture usage (and avoiding gpu hangs) if a shader
    // ever tries to access an invalid resource.
    DX12DescriptorHeap<HeapType::CBV_SRV_UAV, HeapFlags::NONE> nullHeap;

    inline static constexpr u32 DefaultHeapSize = 8192;
    friend class DX12CommandList;
    friend class DX12RHIAccessor;
};
} // namespace vex::dx12