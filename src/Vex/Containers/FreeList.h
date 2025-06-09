#pragma once

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <vector>

#include <Vex/Debug.h>
#include <Vex/Types.h>

namespace vex
{

// Simple index allocator
struct FreeListAllocator
{
    FreeListAllocator(u32 size = 0)
        : size{ size }
    {
        freeIndices.reserve(size);
        for (u32 i = 0; i < size; ++i)
        {
            freeIndices.push_back(size - 1 - i);
        }
    }

    u32 Allocate()
    {
        auto idx = freeIndices.back();
        freeIndices.pop_back();
        return idx;
    }

    void Deallocate(u32 index)
    {
        freeIndices.push_back(index);
        std::ranges::sort(freeIndices, std::greater{});
    }

    void Resize(u32 newSize)
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

    u32 size;
    std::vector<u32> freeIndices;
};

// Free list implementation allowing for allocating and deallocating elements. Handles also store generations, to avoid
// cases of use-after-free.
template <class T, class HandleT>
    requires std::is_default_constructible_v<T>
class FreeList
{
public:
    FreeList(u32 size = 0)
        : values(size)
        , generations(size)
        , allocator(size)
    {
    }

    T& operator[](HandleT handle)
    {
        VEX_ASSERT(IsValid(handle));
        return values[handle.GetIndex()];
    }

    const T& operator[](HandleT handle) const
    {
        VEX_ASSERT(IsValid(handle));
        return values[handle.GetIndex()];
    }

    bool IsValid(HandleT handle) const
    {
        return handle.GetGeneration() == generations[handle.GetIndex()];
    }

    HandleT AllocateElement(T&& elem)
    {
        if (allocator.freeIndices.empty())
        {
            Resize(static_cast<u32>(values.size()) * 2);
        }

        u32 index = allocator.Allocate();
        values[index] = std::move(elem);

        return HandleT::CreateHandle(index, generations[index]);
    }

    void FreeElement(HandleT handle)
    {
        auto idx = handle.GetIndex();
        values[idx] = T{};
        generations[idx]++;
        allocator.Deallocate(idx);
    }

private:
    void Resize(u32 size)
    {
        allocator.Resize(size);
        values.resize(size);
    }

    std::vector<T> values;
    std::vector<u8> generations;
    FreeListAllocator allocator;
};

} // namespace vex