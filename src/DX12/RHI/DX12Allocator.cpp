#include "DX12Allocator.h"

#include <Vex/Logger.h>
#include <Vex/Platform/Platform.h>

#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12Allocator::DX12Allocator(const ComPtr<DX12Device>& device)
    : RHIAllocatorBase(magic_enum::enum_count<HeapType>())
    , device(device)
{
}

Allocation DX12Allocator::AllocateResource(ComPtr<ID3D12Resource>& resource,
                                           const CD3DX12_RESOURCE_DESC& resourceDesc,
                                           HeapType heapType,
                                           D3D12_RESOURCE_STATES initialState,
                                           std::optional<D3D12_CLEAR_VALUE> optionalClearValue)
{
    // Query device for the byte size and alignment of the resource.
    // We cannot compute this ourselves as this depends on hardware/vendors.
    D3D12_RESOURCE_ALLOCATION_INFO allocInfo;

#if defined(_MSC_VER) || !defined(_WIN32)
    allocInfo = device->GetResourceAllocationInfo(0, 1, &resourceDesc);
#else
    device->GetResourceAllocationInfo(&allocInfo,0, 1, &resourceDesc);
#endif

    // Allocates and handles finding an optimal place to allocate the memory.
    // No api calls will be made if a valid MemoryRange is already available, making this super fast!
    Allocation allocation = Allocate(allocInfo.SizeInBytes, allocInfo.Alignment, std::to_underlying(heapType));

#define VEX_DX12_ALLOCATOR_DEBUG_OVERLAPS 0
    // Logs every allocation & detect overlaps (requires disabling the Freeing of resources).
#if VEX_DX12_ALLOCATOR_DEBUG_OVERLAPS
    VEX_LOG(Info,
            "ALLOC: Size={}, Align={}, Offset=0x{:x}, Page={}, HeapType={}\n",
            allocInfo.SizeInBytes,
            allocInfo.Alignment,
            allocation.memoryRange.offset,
            allocation.pageHandle.GetIndex(),
            magic_enum::enum_name(heapType));

    // Check for overlaps
    static std::array<std::vector<std::vector<std::pair<u64, u64>>>, 3>
        allocatedRanges; // offset, size pairs per page per heap
    u64 newStart = allocation.memoryRange.offset;
    u64 newEnd = newStart + allocation.memoryRange.size;
    allocatedRanges[std::to_underlying(heapType)].resize(allocation.pageHandle.GetIndex() + 1);

    for (auto [existingStart, existingSize] :
         allocatedRanges[std::to_underlying(heapType)][allocation.pageHandle.GetIndex()])
    {
        u64 existingEnd = existingStart + existingSize;
        if (!(newEnd <= existingStart || newStart >= existingEnd))
        {
            VEX_LOG(Info,
                    "OVERLAP DETECTED! New[0x{:x}-0x{:x}] vs Existing[0x{:x}-0x{:x}]\n",
                    newStart,
                    newEnd,
                    existingStart,
                    existingEnd);
        }
    }

    allocatedRanges[std::to_underlying(heapType)][allocation.pageHandle.GetIndex()].emplace_back(
        newStart,
        allocation.memoryRange.size);
#endif

    auto& heapList = heaps[std::to_underlying(heapType)];

    chk << device->CreatePlacedResource(heapList[allocation.pageHandle].Get(),
                                        allocation.memoryRange.offset,
                                        &resourceDesc,
                                        initialState,
                                        optionalClearValue.has_value() ? &optionalClearValue.value() : nullptr,
                                        IID_PPV_ARGS(&resource));

    return allocation;
}

void DX12Allocator::FreeResource(const Allocation& allocation)
{
#if VEX_DX12_ALLOCATOR_DEBUG_OVERLAPS
    // Overlap debug requires leaking all resources.
    return;
#endif

    // Frees the underlying range, no api calls will be made if no page needs destroying, making this super fast!
    Free(allocation);
}

void DX12Allocator::OnPageAllocated(PageHandle pageHandle, u32 heapIndex)
{
    auto& heapList = heaps[heapIndex];
    u64 pageByteSize = pageInfos[heapIndex][pageHandle].GetByteSize();

    // Using D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT will default to 64KB, which is valid for all resources APART
    // from MSAA textures.
    u64 heapAlignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED | D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;

    CD3DX12_HEAP_DESC heapDesc;
    switch (static_cast<HeapType>(heapIndex))
    {
    case HeapType::GPUOnly:
        heapDesc = CD3DX12_HEAP_DESC(pageByteSize, D3D12_HEAP_TYPE_DEFAULT, heapAlignment, heapFlags);
        break;
    case HeapType::CPURead:
        heapDesc = CD3DX12_HEAP_DESC(pageByteSize, D3D12_HEAP_TYPE_READBACK, heapAlignment, heapFlags);
        break;
    case HeapType::CPUWrite:
        heapDesc = CD3DX12_HEAP_DESC(pageByteSize, D3D12_HEAP_TYPE_UPLOAD, heapAlignment, heapFlags);
        break;
    default:
        VEX_LOG(Fatal, "Unsupported heapType!");
        break;
    }

    chk << device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heapList[pageHandle]));
#if !VEX_SHIPPING
    chk << heapList[pageHandle]->SetName(
        StringToWString(std::format("AllocatorHeap: {}", magic_enum::enum_name(static_cast<HeapType>(heapIndex))))
            .c_str());
#endif
}

void DX12Allocator::OnPageFreed(PageHandle pageHandle, u32 heapIndex)
{
    // A page is only freed when a resource is freed and its the last occupying the page. Since resource clearing is
    // handled by ResourceCleanup, we can be certain that immediately deleting the heap is safe.
    heaps[heapIndex].erase(pageHandle);
}

} // namespace vex::dx12