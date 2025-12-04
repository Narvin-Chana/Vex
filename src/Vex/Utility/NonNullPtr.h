#pragma once

#include <cstddef>

#include <Vex/Logger.h>

namespace vex
{

template <class T>
class NonNullPtr
{
public:
    // Documents the explicit (and voluntary) deletion of these.
    NonNullPtr() = delete;
    NonNullPtr(std::nullptr_t) = delete;
    NonNullPtr& operator=(std::nullptr_t) = delete;

    // Disallows potentially erroneous validity checking on NonNullPtr.
    explicit operator bool() const = delete;

    explicit NonNullPtr(T* ptr)
        : ptr(ptr)
    {
        if (!ptr)
        {
            VEX_LOG(Fatal, "NonNullPtr was passed a nullptr!");
        }
    }
    NonNullPtr(T& ref)
        : ptr(&ref)
    {
    }
    // This one could potentially be dangerous (although useful), still allowing it for now.
    explicit NonNullPtr(const T& ref)
        : ptr(const_cast<T*>(&ref))
    {
    }
    NonNullPtr(const NonNullPtr&) = default;
    NonNullPtr& operator=(const NonNullPtr&) = default;
    NonNullPtr(NonNullPtr&&) = default;
    NonNullPtr& operator=(NonNullPtr&&) = default;
    ~NonNullPtr() = default;

    // Allows the NonNullPtr to be implicitly convertible to native pointer.
    operator T*() const
    {
        return ptr;
    }

    T* Get()
    {
        return ptr;
    }
    const T* Get() const
    {
        return ptr;
    }

    T& operator*() const
    {
        return *ptr;
    }

    T* operator->() const
    {
        return ptr;
    }

    bool operator==(const NonNullPtr&) const = default;

private:
    T* ptr;
};

} // namespace vex