#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

#include <ExamplePaths.h>

#if VEX_MODULES
import Vex;
#else
#include <Vex.h>
#endif
#include <VexMacros.h>

struct GLFWwindow;

class ExampleApplication
{
public:
    explicit ExampleApplication(std::string_view windowName,
                                int defaultWidth = 0,
                                int defaultHeight = 0,
                                bool allowResize = true);
    virtual ~ExampleApplication();
    virtual void HandleKeyInput(int key, int scancode, int action, int mods);

protected:
    static constexpr int DefaultWidth = 1280, DefaultHeight = 600;

    // Polls window events, then applies any resize that occurred during polling.
    void PollEvents();
    virtual void OnResize(int newWidth, int newHeight);
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

    std::optional<std::pair<int, int>> pendingResize;
};