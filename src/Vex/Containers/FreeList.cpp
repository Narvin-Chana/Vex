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
    auto idx = freeIndices.back();
    freeIndices.pop_back();
    return idx;
}

void FreeListAllocator::Deallocate(u32 index)
{
    freeIndices.push_back(index);
    std::sort(freeIndices.begin(), freeIndices.end(), std::greater{});
}

void FreeListAllocator::Resize(u32 newSize)
{
    if (newSize == size)
    {
        return;
    }

    u32 indicesPreviousSize = static_cast<u32>(freeIndices.size());

    // We only support resizing up!
    freeIndices.resize(indicesPreviousSize + newSize - size);

    u32 idx = newSize - 1;
    u32 value = newSize - size;
    for (u32& handle : freeIndices)
    {
        if (idx == indicesPreviousSize - 1)
        {
            break;
        }
        handle = value;
        idx--;
        value--;
    }
    size = newSize;
}

} // namespace vex