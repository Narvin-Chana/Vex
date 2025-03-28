#pragma once

#include <type_traits>
#include <utility>

namespace vex
{

template <class T>
struct DefaultDeleter
{
    constexpr DefaultDeleter() noexcept = default;

    void operator()(T* ptr) const noexcept
    {
        static_assert(sizeof(T) > 0, "Cannot delete an incomplete type.");
        delete ptr;
    }
};

// TODO: doesn't work if T is an array type, potentially not that important.
template <class T, class Deleter = DefaultDeleter<T>>
class UniqueHandle
{
public:
    constexpr UniqueHandle() = default;
    constexpr explicit UniqueHandle(T* ptr)
        : ptr(ptr)
    {
        static_assert(std::is_array_v<T> == false, "UniqueHandle does not currently support arrays.");
    }

    ~UniqueHandle()
    {
        Deleter{}(ptr);
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept
        : ptr(std::exchange(other.ptr, nullptr))
    {
    }

    UniqueHandle& operator=(UniqueHandle&& other) noexcept
    {
        if (this != &other)
        {
            Deleter{}(ptr);
            ptr = std::exchange(other.ptr, nullptr);
        }
        return *this;
    }

    auto Get() -> T*
    {
        return ptr;
    }

    auto operator->() -> T&
    {
        return *ptr;
    }

private:
    T* ptr = nullptr;
};

template <class T, class... Args>
constexpr UniqueHandle<T> MakeUnique(Args&&... args)
{
    return UniqueHandle<T>(new T(std::forward<Args>(args)...));
}

} // namespace vex