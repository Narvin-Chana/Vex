#include "FreeList.h"

#include <algorithm>

namespace vex
{

FreeListAllocator::FreeListAllocator(u32 size)
    : size{ size }
{
    freeIndices.reserve(size);
    for (u32 i = 0; i < size; ++i)
    {
        freeIndices.push_back(size - 1 - i);
    }
}

u32 FreeListAllocator::Allocate()
{
    if (freeIndices.empty())
    {
        Resize(std::max<u32>(size * 2, 1));
    }

    auto idx = freeIndices.back();
    freeIndices.pop_back();
    return idx;
}

void FreeListAllocator::Deallocate(u32 index)
{
    freeIndices.push_back(index);
    // TODO(https://trello.com/c/6zFzIeVc): investigate whether a quicksort code snippet exists online, to avoid having
    // to include <algorithm> just for it. All current uses of <algorithm> in Vex are only for sorting purposes.
    std::sort(freeIndices.begin(), freeIndices.end(), std::greater{});
}

void FreeListAllocator::Resize(u32 newSize)
{
    if (newSize == size)
    {
        return;
    }

    u32 numNewIndices = newSize - size;
    freeIndices.reserve(freeIndices.size() + numNewIndices);

    // Add new indices
    for (u32 i = size; i < newSize; ++i)
    {
        freeIndices.push_back(i);
    }

    // Re-sort to maintain largest-to-smallest order.
    std::sort(freeIndices.begin(), freeIndices.end(), std::greater{});

    size = newSize;
}

} // namespace vex