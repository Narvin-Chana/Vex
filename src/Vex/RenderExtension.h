#pragma once

#include <Vex/Types.h>

#include <RHI/RHIFwd.h>

namespace vex
{

struct RenderExtensionData
{
    RHI* rhi = nullptr;
    RHIDescriptorPool* descriptorPool = nullptr;
};

class RenderExtension
{
public:
    RenderExtension();
    virtual ~RenderExtension();

    virtual void Initialize()
    {
    }
    virtual void Destroy()
    {
    }

    virtual void OnPrePresent()
    {
    }

    virtual void OnResize(u32 newWidth, u32 newHeight)
    {
    }

    RenderExtensionData data{};
};

} // namespace vex