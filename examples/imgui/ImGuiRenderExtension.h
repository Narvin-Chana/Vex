#pragma once

#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/RenderExtension.h>

struct GLFWwindow;

namespace vex
{
class Graphics;
}

class ImGuiRenderExtension : public vex::RenderExtension
{
public:
    ImGuiRenderExtension(vex::Graphics& graphics,
                         GLFWwindow* window,
                         vex::FrameBuffering buffering,
                         vex::TextureFormat swapchainFormat);

    virtual ~ImGuiRenderExtension() override;

    virtual void Initialize() override;
    virtual void Destroy() override;

    virtual void OnPrePresent() override;

private:
    vex::Graphics& graphics;
    GLFWwindow* window;
    vex::FrameBuffering buffering;
    vex::TextureFormat swapchainFormat;
};