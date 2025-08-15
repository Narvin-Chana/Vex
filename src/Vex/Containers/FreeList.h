#pragma once

#include <utility>
#include <vector>

#include <Vex/Debug.h>
#include <Vex/MaybeUninitialized.h>
#include <Vex/Types.h>

namespace vex
{

// Simple index allocator
struct FreeListAllocator
{
    FreeListAllocator(u32 size = 0);
    u32 Allocate();
    void Deallocate(u32 index);
    void Resize(u32 newSize);

    u32 size;
    std::vector<u32> freeIndices;
};

// Free list implementation allowing for allocating and deallocating elements. Handles also store generations, to avoid
// cases of use-after-free.
template <class T, class HandleT>
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
        VEX_ASSERT(values[handle.GetIndex()].has_value());
        return *values[handle.GetIndex()];
    }

    const T& operator[](HandleT handle) const
    {
        VEX_ASSERT(IsValid(handle));
        VEX_ASSERT(values[handle.GetIndex()].has_value());
        return *values[handle.GetIndex()];
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

        u32 idx = allocator.Allocate();
        VEX_ASSERT(!values[idx].has_value());
        values[idx].emplace(std::move(elem));

        return HandleT::CreateHandle(idx, generations[idx]);
    }

    // Removes an element and destroys it.
    void FreeElement(HandleT handle)
    {
        auto idx = handle.GetIndex();
        VEX_ASSERT(values[idx].has_value());
        values[idx].reset();
        generations[idx]++;
        allocator.Deallocate(idx);
    }

    // Extracts an element, removing it from the freelist without destroying it.
    MaybeUninitialized<T> ExtractElement(HandleT handle)
    {
        auto idx = handle.GetIndex();
        generations[idx]++;
        allocator.Deallocate(idx);
        VEX_ASSERT(values[idx].has_value());
        return std::exchange(values[idx], std::nullopt);
    }

private:
    void Resize(u32 size)
    {
        allocator.Resize(size);
        values.resize(size);
    }

    std::vector<MaybeUninitialized<T>> values;
    std::vector<u8> generations;
    FreeListAllocator allocator;
};

} // namespace vex