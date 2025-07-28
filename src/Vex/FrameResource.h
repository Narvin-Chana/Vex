#pragma once

#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include <Vex/Types.h>

namespace vex
{

// Determines how many frames should be in flight at once.
// More frames in flight means less GPU starvation, but also more input latency.
// Single buffering is not supported due to the APIs Vex supports not allowing for swapchains of less than 2
// backbuffers.
enum class FrameBuffering : u8
{
    // Two frames in flight at once
    Double = 2,
    // Three frames in flight at once
    Triple = 3
};

// For resources who exist once per frame buffer count (eg: command pools, constant buffers, structured buffer).
template <class T>
    requires std::is_default_constructible_v<T>
class FrameResource
{
public:
    FrameResource(FrameBuffering frameBuffering)
        : resource(std::to_underlying(frameBuffering))
    {
    }
    ~FrameResource() = default;

    T& Get(u32 frameIndex)
    {
        return resource[frameIndex];
    }

    const T& Get(u32 frameIndex) const
    {
        return resource[frameIndex];
    }

    void ForEach(std::function<void(T&)> func)
    {
        for (auto& el : resource)
        {
            func(el);
        }
    }

    void ForEach(std::function<void(const T&)> func) const
    {
        for (const auto& el : resource)
        {
            func(el);
        }
    }

private:
    std::vector<T> resource;
};

} // namespace vex