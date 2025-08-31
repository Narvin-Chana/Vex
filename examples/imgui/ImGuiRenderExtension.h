#pragma once

#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/RenderExtension.h>

struct GLFWwindow;

namespace vex
{
class GfxBackend;
}

class ImGuiRenderExtension : public vex::RenderExtension
{
public:
    ImGuiRenderExtension(vex::GfxBackend& graphics,
                         GLFWwindow* window,
                         vex::FrameBuffering buffering,
                         vex::TextureFormat swapchainFormat);

    virtual ~ImGuiRenderExtension() override;

    virtual void Initialize() override;
    virtual void Destroy() override;

    virtual void OnPrePresent() override;

private:
    vex::GfxBackend& graphics;
    GLFWwindow* window;
    vex::FrameBuffering buffering;
    vex::TextureFormat swapchainFormat;
};