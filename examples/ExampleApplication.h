#pragma once

#include <string_view>

#include <Vex.h>

class GLFWwindow;

class ExampleApplication
{
public:
    ExampleApplication(std::string_view windowName);
    virtual ~ExampleApplication();

    virtual void HandleKeyInput(int key, int scancode, int action, int mods) = 0;

protected:
    static constexpr int32_t DefaultWidth = 1280, DefaultHeight = 600;

    virtual void OnResize(GLFWwindow* window, uint32_t newWidth, uint32_t newHeight);
    void ToggleFullscreen();

    enum WindowMode
    {
        Windowed,
        Fullscreen,
    } windowMode = Windowed;

    struct WindowedInfo
    {
        int32_t width, height, x, y;
    } windowedInfo;

    int32_t width, height;

    GLFWwindow* window;
    vex::UniqueHandle<vex::GfxBackend> graphics;
};