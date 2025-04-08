#pragma once

#include <type_traits>
#include <utility>
#include <vector>

#include <Vex/Types.h>

namespace vex
{

// Determines how many frames should be in flight at once.
// More frames in flight means less GPU starvation, but also more input latency.
enum class FrameBuffering
{
    // One frame in flight at once
    Single = 1,
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

private:
    std::vector<T> resource;
};

} // namespace vex