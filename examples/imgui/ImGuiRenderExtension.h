#pragma once

#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/RenderExtension.h>

namespace vex
{
class GfxBackend;
}

struct GLFWwindow;

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

    virtual void OnFrameStart() override;
    virtual void OnFrameEnd() override;

private:
    vex::GfxBackend& graphics;
    GLFWwindow* window;
    vex::FrameBuffering buffering;
    vex::TextureFormat swapchainFormat;
};