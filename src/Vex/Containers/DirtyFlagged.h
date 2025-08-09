#pragma once
#include <utility>

namespace vex
{

template <class T>
class DirtyFlagged
{
    mutable bool dirtyFlag = true;
    T internalValue;

public:
    DirtyFlagged() = default;

    DirtyFlagged(const T& value)
        : internalValue{ value }
    {
    }

    DirtyFlagged(T&& value)
        : internalValue{ std::move(value) }
    {
    }

    DirtyFlagged(const DirtyFlagged& other)
        : DirtyFlagged{ other.internalValue }
    {
    }
    DirtyFlagged(DirtyFlagged&& other) noexcept
        : DirtyFlagged{ std::move(other.internalValue) }
    {
    }

    DirtyFlagged& operator=(const DirtyFlagged& other)
    {
        dirtyFlag = true;
        internalValue = other.internalValue;
        return *this;
    }

    DirtyFlagged& operator=(DirtyFlagged&& other) noexcept
    {
        dirtyFlag = true;
        internalValue = std::move(other.internalValue);
        return *this;
    }

    T& operator*()
    {
        return internalValue;
    }
    T* operator->()
    {
        return &internalValue;
    }

    T& Value()
    {
        return internalValue;
    }
    const T& Value() const
    {
        return internalValue;
    }
    bool IsDirty() const noexcept
    {
        return dirtyFlag;
    }
    void ClearDirty() const
    {
        dirtyFlag = false;
    }
    bool CheckAndClear() const
    {
        const bool isDirty = dirtyFlag;
        dirtyFlag = false;
        return isDirty;
    }
};

} // namespace vex
