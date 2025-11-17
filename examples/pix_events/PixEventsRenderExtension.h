#pragma once

#include <string_view>

#include <Vex/RenderExtension.h>

namespace vex
{
struct PlatformWindowHandle;
}

class PixEventsRenderExtension : public vex::RenderExtension
{
public:
    PixEventsRenderExtension();
    static void StartCapture(std::wstring_view captureName);
    static void EndCapture();
};