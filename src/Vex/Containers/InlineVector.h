#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include <Vex/Logger.h>
#include <Vex/Types.h>
#include <VexMacros.h>

namespace vex
{

// Variable size array, allocated on the stack (backed by a std::array).
// Useful for when the upper limit of a vector is known at compile-time to avoid dynamic memory allocation.
// TODO: C++26 will allow us to use std::inplace_vector, this class will then be able to be completely removed.
template <class T, std::size_t N>
class InlineVector
{
public:
    constexpr void push_back(const T& v)
    {
        VEX_ASSERT(count < N);
        data[count++] = v;
    }
    template <class... Args>
    constexpr void emplace_back(Args&&... args)
    {
        VEX_ASSERT(count < N);
        data[count++] = T(std::forward<Args>(args)...);
    }

    constexpr std::size_t size() const
    {
        return count;
    }
    static constexpr std::size_t capacity()
    {
        return N;
    }
    constexpr bool empty() const
    {
        return size() == 0;
    }

    constexpr const T* begin() const
    {
        return data.data();
    }
    constexpr const T* end() const
    {
        return data.data() + size();
    }
    constexpr T* begin()
    {
        return data.data();
    }
    constexpr T* end()
    {
        return data.data() + size();
    }

    constexpr const T& operator[](std::size_t index) const
    {
        VEX_ASSERT(index < size());
        return data[index];
    }
    constexpr T& operator[](std::size_t index)
    {
        VEX_ASSERT(index < size());
        return data[index];
    }

    constexpr void resize(std::size_t newCount)
    {
        VEX_ASSERT(newCount <= N);
        // Initialize new data members to default if the list grew in size.
        for (u64 i = count; i < newCount; ++i)
        {
            data[i] = T{};
        }
        // Reset data members to default if the list shrank in size.
        for (u64 i = newCount; i < count; ++i)
        {
            data[i] = T{};
        }
        count = newCount;
    }

    // We can add other members as needed...
    // For now this is just a lightweight wrapper.

    constexpr bool operator==(const InlineVector& other) const
    {
        return count == other.count && std::equal(begin(), end(), other.begin());
    }

    std::array<T, N> data;
    u64 count = 0;
};

} // namespace vex