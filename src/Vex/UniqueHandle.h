#pragma once

#include <memory>
#include <type_traits>

namespace vex
{

template <class T, class Deleter = std::default_delete<T>>
using UniqueHandle = std::unique_ptr<T, Deleter>;

template <class T, class... Args>
    requires(not std::is_array_v<T>)
UniqueHandle<T> MakeUnique(Args&&... args)
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <class T>
    requires(std::is_unbounded_array_v<T>)
UniqueHandle<T> MakeUnique(std::size_t size)
{
    return std::make_unique<T>(size);
}

template <class T, class... Args>
    requires(std::is_bounded_array_v<T>)
UniqueHandle<T> MakeUnique(Args&&...) = delete;

} // namespace vex