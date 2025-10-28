#pragma once

#include <unordered_map>

#include <Vex/Resource.h>

#include <RHI/RHIAllocator.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

using HeapType = ResourceMemoryLocality;

class DX12Allocator : public RHIAllocatorBase
{
public:
    DX12Allocator(const ComPtr<DX12Device>& device);

    Allocation AllocateResource(ComPtr<ID3D12Resource>& resource,
                                const CD3DX12_RESOURCE_DESC& resourceDesc,
                                HeapType heapType,
                                D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON,
                                std::optional<D3D12_CLEAR_VALUE> optionalClearValue = std::nullopt);
    void FreeResource(const Allocation& allocation);

protected:
    virtual void OnPageAllocated(PageHandle pageHandle, u32 heapIndex) override;
    virtual void OnPageFreed(PageHandle pageHandle, u32 heapIndex) override;

private:
    ComPtr<DX12Device> device;

    // clang-format off
    
    // Contains all API-specific data for a specific freeList handle.
    std::array<std::unordered_map<PageHandle, ComPtr<ID3D12Heap>>, magic_enum::enum_count<HeapType>()> heaps;

    // clang-format on

    // TODO: Add support for GPU_UPLOAD heap for ultra-fast upload using ReBar. Requires device querying to determine
    // rebar size.
};

} // namespace vex::dx12