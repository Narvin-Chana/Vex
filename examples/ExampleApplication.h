#pragma once

#include <filesystem>
#include <string_view>

#include <Vex.h>
#include <Vex/Logger.h>
#include <ExamplePaths.h>

struct GLFWwindow;

class ExampleApplication
{
public:
    ExampleApplication(std::string_view windowName, int defaultWidth = 0, int defaultHeight = 0, bool allowResize = true);
    virtual ~ExampleApplication();
    virtual void HandleKeyInput(int key, int scancode, int action, int mods);

protected:
    static constexpr int DefaultWidth = 1280, DefaultHeight = 600;

    virtual void OnResize(GLFWwindow* window, int newWidth, int newHeight);
    void ToggleFullscreen();
    void SetupShaderErrorHandling();

    vex::PlatformWindowHandle GetPlatformWindowHandle() const;

    enum WindowMode
    {
        Windowed,
        Fullscreen,
    } windowMode = Windowed;

    struct WindowedInfo
    {
        int width, height, x, y;
    } windowedInfo;

    int width, height;

    GLFWwindow* window;
    std::unique_ptr<vex::Graphics> graphics;
    vex::ShaderCompiler shaderCompiler;
};