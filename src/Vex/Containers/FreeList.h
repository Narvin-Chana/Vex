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
    FreeListAllocator(size_t size = 0)
        : size{ size }
    {
        freeIndices.reserve(size);
        for (i32 i = size - 1; i > 0; --i)
        {
            freeIndices.push_back(static_cast<u32>(i));
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

    void Resize(size_t newSize)
    {
        if (newSize == size)
        {
            return;
        }

        size_t indicesPreviousSize = freeIndices.size();

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

    size_t size;
    std::vector<u32> freeIndices;
};

// Free list implementation allowing for allocating and deallocating elements. Handles also store generations, to avoid
// cases of use-after-free.
template <class T, class HandleT>
    requires std::is_default_constructible_v<T>
class FreeList
{
public:
    FreeList(size_t size = 0)
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
            Resize(values.size() * 2);
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
    void Resize(size_t size)
    {
        allocator.Resize(size);
        values.resize(size);
    }

    std::vector<T> values;
    std::vector<u8> generations;
    FreeListAllocator allocator;
};

} // namespace vex