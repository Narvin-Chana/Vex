#pragma once

#include <ExampleApplication.h>

struct PixEventsApplication : public ExampleApplication
{
    PixEventsApplication();
    void Run();

protected:
    static constexpr auto FrameBuffering = vex::FrameBuffering::Triple;
    static constexpr auto SwapchainFormat = vex::TextureFormat::RGBA8_UNORM;

    virtual void OnResize(GLFWwindow* window, uint32_t width, uint32_t height) override;
};