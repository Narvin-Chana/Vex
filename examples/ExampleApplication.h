#pragma once

#include <Vex.h>

class GLFWwindow;

class ExampleApplication
{
public:
    ExampleApplication();
    virtual ~ExampleApplication();

protected:
    static constexpr int32_t DefaultWidth = 1280, DefaultHeight = 600;

    void OnResize(GLFWwindow* window, uint32_t width, uint32_t height);
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