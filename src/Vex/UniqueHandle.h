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

template <class T>
struct DefaultDeleter<T[]>
{
    constexpr DefaultDeleter() noexcept = default;

    void operator()(T* ptr) const noexcept
    {
        static_assert(sizeof(T) > 0, "Cannot delete an incomplete type.");
        delete[] ptr;
    }
};

template <class T, class Deleter = DefaultDeleter<T>>
class UniqueHandle;

template <class T, class Deleter>
class UniqueHandle
{
public:
    constexpr UniqueHandle() = default;
    constexpr explicit UniqueHandle(T* ptr)
        : ptr(ptr)
    {
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

    T* operator->() const noexcept
    {
        return ptr;
    }

    T& operator*() const noexcept
    {
        return *ptr;
    }

    void Reset(T* newPtr = nullptr) noexcept
    {
        Deleter{}(ptr);
        ptr = newPtr;
    }

    T* Release() noexcept
    {
        return std::exchange(ptr, nullptr);
    }

    explicit operator bool() const noexcept
    {
        return ptr != nullptr;
    }

    T* Get()
    {
        return ptr;
    }

private:
    T* ptr = nullptr;
};

template <class T, class Deleter>
class UniqueHandle<T[], Deleter>
{
public:
    constexpr UniqueHandle() = default;
    constexpr explicit UniqueHandle(T* ptr)
        : ptr(ptr)
    {
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

    T* Get()
    {
        return ptr;
    }

    T& operator*() const noexcept
    {
        return *ptr;
    }

    T& operator[](std::size_t i) const noexcept
    {
        return ptr[i];
    }

    void Reset(T* newPtr = nullptr) noexcept
    {
        Deleter{}(ptr);
        ptr = newPtr;
    }

    T* Release() noexcept
    {
        return std::exchange(ptr, nullptr);
    }

    explicit operator bool() const noexcept
    {
        return ptr != nullptr;
    }

private:
    T* ptr = nullptr;
};

template <class T, class... Args>
    requires(not std::is_array_v<T>)
constexpr UniqueHandle<T> MakeUnique(Args&&... args)
{
    return UniqueHandle<T>(new T(std::forward<Args>(args)...));
}

template <class T>
    requires(std::is_unbounded_array_v<T>)
constexpr UniqueHandle<T> MakeUnique(std::size_t size)
{
    using ElementType = std::remove_extent_t<T>;
    return UniqueHandle<T>(new ElementType[size]);
}

template <class T, class... Args>
    requires(std::is_bounded_array_v<T>)
constexpr UniqueHandle<T> MakeUnique(Args&&...) = delete;

} // namespace vex