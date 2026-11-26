#pragma once

#include <ExampleApplication.h>

struct ImGuiApplication : ExampleApplication
{
    ImGuiApplication();
    virtual ~ImGuiApplication() override;
    void Run();
    void RenderImGui();

protected:
    vex::Texture lastFrameTexture;

    static constexpr auto FrameBuffering = vex::FrameBuffering::Triple;
    static constexpr auto SwapchainFormat = vex::TextureFormat::BGRA8_UNORM;

    virtual void OnResize(int width, int height) override;
};