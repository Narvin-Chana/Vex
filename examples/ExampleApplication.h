#pragma once

#include <filesystem>
#include <functional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

#include <ExamplePaths.h>

// importing in a header file can cause mangling issues in GCC
#if VEX_MODULES && !(__GNUC__ && !__clang__)
import Vex.ShaderCompiler;
#else
#include <VexShaderCompiler.h>
#endif
#include <VexMacros.h>

struct GLFWwindow;

class ExampleApplication
{
public:
    ExampleApplication(std::string_view windowName,
                       int defaultWidth = 0,
                       int defaultHeight = 0,
                       bool allowResize = true);
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