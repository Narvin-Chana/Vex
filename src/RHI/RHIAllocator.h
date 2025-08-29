#pragma once

#include <vector>

#include <Vex/Containers/FreeList.h>
#include <Vex/Handle.h>
#include <Vex/MemoryAllocation.h>
#include <Vex/Types.h>

namespace vex
{

// Provides simple CPU-side tracking logic for allocating memory ranges inside memory pages (default size of 256MB per
// page).
class RHIAllocatorBase
{
protected:
    RHIAllocatorBase(u32 memoryTypeCount);

    Allocation Allocate(u64 size, u64 alignment, u32 memoryTypeIndex);
    void Free(const Allocation& allocation);

    // Will perform the actual API calls to allocate/deallocate pages.
    virtual void OnPageAllocated(PageHandle handle, u32 memoryTypeIndex) = 0;
    virtual void OnPageFreed(PageHandle handle, u32 memoryTypeIndexs) = 0;

    std::vector<FreeList<MemoryPageInfo, PageHandle>> pageInfos;
};

} // namespace vex