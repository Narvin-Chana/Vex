#pragma once

#include <vector>

#include <Vex/Containers/FreeList.h>
#include <Vex/Handle.h>
#include <Vex/Types.h>

#if defined(_WIN32)
#ifdef GetFreeSpace
#undef GetFreeSpace
#endif
#endif

namespace vex
{

struct MemoryRange
{
    u64 offset;
    u64 size;

    bool operator==(const MemoryRange& other) const = default;

    // When sorting only the offset has importance.
    auto operator<=>(const MemoryRange& other) const
    {
        return offset <=> other.offset;
    }

    u64 end() const
    {
        return offset + size;
    }
};

struct MemoryPageInfo
{
    // Vex allocates pages of a default size of 256MB.
    static constexpr u64 DefaultPageByteSize = 256 * 1024 * 1024;

    MemoryPageInfo(u32 memoryTypeIndex, u64 pageByteSize = DefaultPageByteSize);

    std::optional<MemoryRange> Allocate(u64 size, u64 alignment);
    void Free(const MemoryRange& range);

    [[nodiscard]] u64 GetByteSize() const
    {
        return pageByteSize;
    }

    [[nodiscard]] u64 GetFreeSpace() const
    {
        u64 totalAllocated = 0;
        for (const auto& range : allocatedRanges)
        {
            totalAllocated += range.size;
        }
        return pageByteSize - totalAllocated;
    }

private:
    // Searches for the first range that contains enough space to fit the requested data and, if found, returns the
    // memory range.
    std::optional<MemoryRange> FindFreeSpace(u64 size, u64 alignment);

    u32 memoryTypeIndex;
    u64 pageByteSize;
    std::vector<MemoryRange> allocatedRanges;
};

struct PageHandle : public Handle<PageHandle>
{
};

static constexpr PageHandle GInvalidPageHandle;

struct Allocation
{
    u32 memoryTypeIndex;
    PageHandle pageHandle = GInvalidPageHandle;
    MemoryRange memoryRange;
};

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