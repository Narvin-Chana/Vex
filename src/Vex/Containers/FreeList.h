#pragma once

#include <algorithm>
#include <ranges>
#include <type_traits>
#include <vector>

#include <Vex/Types.h>

namespace vex
{

template <class T, class HandleT>
    requires std::is_default_constructible_v<T>
class FreeList
{
public:
    FreeList(size_t size = 0)
        : values(size)
        , freeHandles(size)
    {
        u32 idx = size - 1;
        for (HandleT& handle : freeHandles)
        {
            handle.SetHandle(idx, 0);
            idx--;
        }
    }
    T& operator[](HandleT handle)
    {
        return values[handle.GetIndex()];
    }

    const T& operator[](HandleT handle) const
    {
        return values[handle.GetIndex()];
    }

    HandleT AllocateElement(T&& elem)
    {
        if (freeHandles.empty())
        {
            Resize(values.size() * 2);
        }
        HandleT handle = freeHandles.back();
        freeHandles.pop_back();
        values[handle.GetIndex()] = std::move(elem);

        return handle;
    }

    void FreeElement(HandleT handle)
    {
        values[handle.GetIndex()] = T{};
        handle.IncrementGeneration();
        freeHandles.push_back(handle);
        std::ranges::sort(freeHandles, std::greater{});
    }

private:
    void Resize(size_t size)
    {
        size_t prevSize = values.size();
        if (size == prevSize)
        {
            return;
        }

        size_t indicesPreviousSize = freeHandles.size();

        // We only support resizing up!
        freeHandles.resize(indicesPreviousSize + size - prevSize);

        u32 idx = size - 1;
        u32 value = size - prevSize;
        for (HandleT& handle : freeHandles)
        {
            if (idx == indicesPreviousSize - 1)
            {
                break;
            }
            handle.SetHandle(value, 0);
            idx--;
            value--;
        }
        values.resize(size);
    }

    std::vector<T> values;
    std::vector<HandleT> freeHandles;
};

} // namespace vex