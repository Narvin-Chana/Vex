#pragma once

#include "../ExampleApplication.h"

class GLFWwindow;

struct ImGuiApplication : public ExampleApplication
{
    ImGuiApplication();
    virtual ~ImGuiApplication() override;
    virtual void HandleKeyInput(int key, int scancode, int action, int mods) override;

    void Run();

protected:
    static constexpr auto FrameBuffering = vex::FrameBuffering::Triple;
    static constexpr auto SwapchainFormat = vex::TextureFormat::RGBA8_UNORM;

    virtual void OnResize(GLFWwindow* window, uint32_t width, uint32_t height) override;
};