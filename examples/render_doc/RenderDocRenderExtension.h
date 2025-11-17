#pragma once

#include <Vex/RenderExtension.h>

namespace vex
{
struct PlatformWindowHandle;
}

class RenderDocRenderExtension : public vex::RenderExtension
{
public:
    RenderDocRenderExtension(const vex::PlatformWindowHandle& handle);
    virtual void Initialize() override;

    static void StartCapture();
    static void EndCapture();
};