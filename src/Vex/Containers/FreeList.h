#pragma once

#include <algorithm>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <Vex/Containers/Span.h>
#include <Vex/Platform/Debug.h>
#include <Vex/Types.h>
#include <Vex/Utility/MaybeUninitialized.h>

namespace vex
{

// Simple free-list index allocator
template <class IndexT = u32>
    requires std::is_integral_v<IndexT>
struct FreeListAllocator
{
    FreeListAllocator(IndexT size = 0)
        : size{ size }
    {
        freeIndices.reserve(size);
        for (IndexT i = 0; i < size; ++i)
        {
            freeIndices.push_back(size - 1 - i);
        }
    }

    IndexT Allocate()
    {
        if (freeIndices.empty())
        {
            Resize(std::max<IndexT>(size * 2, 1));
        }

        IndexT idx = freeIndices.back();
        freeIndices.pop_back();
        return idx;
    }

    void DeallocateBatch(Span<IndexT> indices)
    {
        if (!indices.empty())
        {
            freeIndices.insert(freeIndices.end(), indices.begin(), indices.end());
            std::sort(freeIndices.begin(), freeIndices.end(), std::greater{});
        }
    }

    void Deallocate(IndexT index)
    {
        freeIndices.push_back(index);
        std::sort(freeIndices.begin(), freeIndices.end(), std::greater{});
    }

    void Resize(IndexT newSize)
    {
        if (newSize == size)
        {
            return;
        }

        IndexT numNewIndices = newSize - size;
        freeIndices.reserve(freeIndices.size() + numNewIndices);

        // Add new indices
        for (IndexT i = size; i < newSize; ++i)
        {
            freeIndices.push_back(i);
        }

        // Re-sort to maintain largest-to-smallest order.
        std::sort(freeIndices.begin(), freeIndices.end(), std::greater{});

        size = newSize;
    }

    IndexT size;
    std::vector<IndexT> freeIndices;
};

// 32 bit free-list allocator.
using FreeListAllocator32 = FreeListAllocator<u32>;

// 64 bit free-list allocator.
using FreeListAllocator64 = FreeListAllocator<u64>;

// Free list implementation allowing for allocating and deallocating elements. Handles also store generations, to avoid
// cases of use-after-free.
template <class T, class HandleT>
class FreeList
{
public:
    using IndexT = HandleT::ValueType;

    FreeList(IndexT size = 0)
        : values(size)
        , generations(size)
        , allocator(size)
    {
    }

    T& operator[](HandleT handle)
    {
        VEX_ASSERT(IsValid(handle), "Invalid handle passed to freelist: {}", handle);
        VEX_ASSERT(values[handle.GetIndex()].has_value(), "Invalid handle passed to freelist: {}", handle);
        return *values[handle.GetIndex()];
    }

    const T& operator[](HandleT handle) const
    {
        VEX_ASSERT(IsValid(handle), "Invalid handle passed to freelist: {}", handle);
        VEX_ASSERT(values[handle.GetIndex()].has_value(), "Invalid handle passed to freelist: {}", handle);
        return *values[handle.GetIndex()];
    }

    bool IsValid(HandleT handle) const
    {
        return handle.IsValid() && handle.GetGeneration() == generations[handle.GetIndex()];
    }

    HandleT AllocateElement(T elem)
    {
        if (allocator.freeIndices.empty())
        {
            Resize(std::max<IndexT>(static_cast<IndexT>(values.size()) * 2, 1));
        }

        IndexT idx = allocator.Allocate();
        VEX_ASSERT(!values[idx].has_value(),
                   "Error: freelist free slot and values do not match up, trying to create an element in a slot which "
                   "already contains a valid element.");
        values[idx].emplace(std::move(elem));

        return HandleT::CreateHandle(idx, generations[idx]);
    }

    void FreeElementBatch(std::span<HandleT> elements)
    {
        std::vector<IndexT> indices;
        indices.reserve(elements.size());
        for (HandleT handle : elements)
        {
            IndexT idx = handle.GetIndex();
            VEX_ASSERT(values[idx].has_value(), "Error: trying to free an element which does not exist.");
            values[idx].reset();
            generations[idx]++;
            indices.push_back(idx);
        }
        allocator.DeallocateBatch(indices);
    }

    // Removes an element and destroys it.
    void FreeElement(HandleT handle)
    {
        IndexT idx = handle.GetIndex();
        VEX_ASSERT(values[idx].has_value(), "Error: trying to free an element which does not exist.");
        values[idx].reset();
        generations[idx]++;
        allocator.Deallocate(idx);
    }

    // Extracts an element, removing it from the freelist without destroying it.
    MaybeUninitialized<T> ExtractElement(HandleT handle)
    {
        IndexT idx = handle.GetIndex();
        generations[idx]++;
        allocator.Deallocate(idx);
        VEX_ASSERT(values[idx].has_value(), "Error: trying to extract an element which does not exist.");
        return std::exchange(values[idx], std::nullopt);
    }

    IndexT ElementCount() const
    {
        return allocator.size - allocator.freeIndices.size();
    }

    void Resize(IndexT newSize)
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
        IndexT index;
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
        return iterator(this, static_cast<IndexT>(values.size()));
    }
    const_iterator begin() const
    {
        return const_iterator(this, 0);
    }
    const_iterator end() const
    {
        return const_iterator(this, static_cast<IndexT>(values.size()));
    }

private:
    std::vector<MaybeUninitialized<T>> values;
    std::vector<IndexT> generations;
    FreeListAllocator<IndexT> allocator;
};

} // namespace vex