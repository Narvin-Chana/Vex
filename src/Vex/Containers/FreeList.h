#pragma once

#include <utility>
#include <vector>

#include <Vex/Platform/Debug.h>
#include <Vex/Types.h>
#include <Vex/Utility/MaybeUninitialized.h>

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

    HandleT AllocateElement(T elem)
    {
        if (allocator.freeIndices.empty())
        {
            Resize(std::max<u32>(static_cast<u32>(values.size()) * 2, 1));
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

    u32 ElementCount() const
    {
        return allocator.size - allocator.freeIndices.size();
    }

    void Resize(u32 newSize)
    {
        allocator.Resize(newSize);
        generations.resize(newSize);
        values.resize(newSize);
    }

    // This iterator will skip over elements in values which have not been initialized.
    template <bool IsConst>
    class Iterator
    {
        using ListType = std::conditional_t<IsConst, const FreeList, FreeList>;
        using ValueType = std::conditional_t<IsConst, const T, T>;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = ValueType*;
        using reference = ValueType&;

        Iterator(ListType* list, u32 index)
            : list(list)
            , index(index)
        {
            SkipEmpty();
        }

        // Allows us to access the handle from the iterator.
        HandleT GetHandle() const
        {
            return HandleT::CreateHandle(index, list->generations[index]);
        }

        reference operator*() const
        {
            return *list->values[index];
        }
        pointer operator->() const
        {
            return &(*list->values[index]);
        }

        Iterator& operator++()
        {
            ++index;
            SkipEmpty();
            return *this;
        }

        bool operator==(const Iterator& other) const
        {
            return index == other.index;
        }

    private:
        void SkipEmpty()
        {
            while (index < list->values.size() && !list->values[index].has_value())
                ++index;
        }

        ListType* list;
        u32 index;
    };

    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    // Iterator interface
    iterator begin()
    {
        return iterator(this, 0);
    }
    iterator end()
    {
        return iterator(this, static_cast<u32>(values.size()));
    }
    const_iterator begin() const
    {
        return const_iterator(this, 0);
    }
    const_iterator end() const
    {
        return const_iterator(this, static_cast<u32>(values.size()));
    }

private:
    std::vector<MaybeUninitialized<T>> values;
    std::vector<u8> generations;
    FreeListAllocator allocator;
};

} // namespace vex