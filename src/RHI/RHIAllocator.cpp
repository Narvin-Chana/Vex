#include "RHIAllocator.h"

#include <algorithm>

#include <Vex/ByteUtils.h>
#include <Vex/Logger.h>

namespace vex
{

MemoryPageInfo::MemoryPageInfo(HeapType heapType, u64 pageByteSize)
    : heapType(heapType)
    , pageByteSize(pageByteSize)
{
}

std::optional<MemoryRange> MemoryPageInfo::Allocate(u64 size, u64 alignment)
{
    auto range = FindFreeSpace(size, alignment);
    if (!range.has_value())
    {
        return std::nullopt;
    }
    allocatedRanges.emplace_back(range.value());

    // Keep allocated ranges sorted for easier traversal.
    // TODO(https://trello.com/c/6zFzIeVc): investigate whether a quicksort code snippet exists online, to avoid having
    // to include <algorithm> just for it. All current uses of <algorithm> in Vex are only for sorting purposes.
    std::sort(allocatedRanges.begin(), allocatedRanges.end(), std::less<MemoryRange>{});

    return range.value();
}

void MemoryPageInfo::Free(const MemoryRange& range)
{
    std::erase(allocatedRanges, range);
}

// Searches for the first range that contains enough space to fit the requested data and, if found, returns the
// memory range.
std::optional<MemoryRange> MemoryPageInfo::FindFreeSpace(u64 size, u64 alignment)
{
    u64 searchOffset = 0;

    // Check in between currently allocated ranges.
    for (auto& range : allocatedRanges)
    {
        u64 alignedOffset = AlignUp(searchOffset, alignment);

        if (alignedOffset + size <= range.offset)
        {
            return MemoryRange{ .offset = alignedOffset, .size = size };
        }

        // Move to searchOffset the end of this range, and check the next one!
        searchOffset = range.end();
    }

    // Check after the last allocated range.
    u64 alignedOffset = AlignUp(searchOffset, alignment);
    if (alignedOffset + size <= pageByteSize)
    {
        return MemoryRange{ .offset = alignedOffset, .size = size };
    }

    // No space found!
    return std::nullopt;
}

Allocation RHIAllocatorBase::Allocate(u64 size, u64 alignment, HeapType heapType)
{
    u64 alignedSize = AlignUp(size, alignment);

    auto& memoryPages = pageInfos[std::to_underlying(heapType)];
    for (auto it = memoryPages.begin(); it != memoryPages.end(); ++it)
    {
        // If page is too small, skip it as there's no chance it will allow us to allocate.
        if (alignedSize > it->GetFreeSpace())
        {
            continue;
        }

        if (auto res = it->Allocate(size, alignment))
        {
#if !VEX_SHIPPING
            VEX_LOG(Verbose, "Allocated range: size {} offset {}", res.value().size, res.value().offset);
#endif

            return Allocation{
                .heapType = heapType,
                .pageHandle = it.GetHandle(),
                .memoryRange = res.value(),
            };
        }
    }

    // No valid page was found, we have to create a new page.
    PageHandle pageHandle = memoryPages.AllocateElement(
        MemoryPageInfo(heapType, std::max(alignedSize, MemoryPageInfo::DefaultPageByteSize)));
#if !VEX_SHIPPING
    VEX_LOG(Verbose,
            "Allocated new page: size {} alignment {}!",
            std::max(alignedSize, MemoryPageInfo::DefaultPageByteSize),
            alignment);
#endif

    if (auto res = memoryPages[pageHandle].Allocate(size, alignment))
    {
#if !VEX_SHIPPING
        VEX_LOG(Verbose, "Allocated range: size {} offset {}", res.value().size, res.value().offset);
#endif
        OnPageAllocated(pageHandle, heapType);
        return Allocation{
            .heapType = heapType,
            .pageHandle = pageHandle,
            .memoryRange = res.value(),
        };
    }

    VEX_LOG(Fatal,
            "The program was unable to fit the requested allocation in any existing pages AND was unable to "
            "allocate a new page for: size {} and alignment {} on heapType: {}!",
            size,
            alignment,
            magic_enum::enum_name(heapType));
    std::unreachable();
}

void RHIAllocatorBase::Free(const Allocation& allocation)
{
    auto& memoryPages = pageInfos[std::to_underlying(allocation.heapType)];
    auto& page = memoryPages[allocation.pageHandle];
#if !VEX_SHIPPING
    VEX_LOG(Verbose, "Freed range: size {} offset {}", allocation.memoryRange.size, allocation.memoryRange.offset);
#endif
    page.Free(allocation.memoryRange);

    // If the page is a non-default sized page and was completely freed up (this was the last resource inside the
    // page), we can delete it.
    if (page.GetByteSize() != page.DefaultPageByteSize && page.GetFreeSpace() == page.GetByteSize())
    {
#if !VEX_SHIPPING
        VEX_LOG(Verbose, "Freed page: size {}", page.GetByteSize());
#endif

        OnPageFreed(allocation.pageHandle, allocation.heapType);
        memoryPages.FreeElement(allocation.pageHandle);
    }

    // Default sized pages persist, even when empty, as they will be the place where most memory gets stored.
    // TBD if we want to have some tracking to also free those, right now its good enough to just keep expanding on
    // demand.
}

} // namespace vex