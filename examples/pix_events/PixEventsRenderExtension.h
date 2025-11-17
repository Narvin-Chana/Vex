#pragma once

#include <Vex/RenderExtension.h>

namespace vex
{
struct PlatformWindowHandle;
}

class PixEventsRenderExtension : public vex::RenderExtension
{
public:
    PixEventsRenderExtension();
    static void StartCapture();
    static void EndCapture();
};