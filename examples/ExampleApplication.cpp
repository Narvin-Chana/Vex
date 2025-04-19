#include "ExampleApplication.h"

#include <functional>

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3native.h>

#include <Vex.h>
#include <Vex/Logger.h>

ExampleApplication::ExampleApplication()
{
#if defined(__linux__)
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

    if (!glfwInit())
    {
        VEX_LOG(vex::Fatal, "Unable to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(DefaultWidth, DefaultHeight, "ExampleApplication", nullptr, nullptr);

    width = DefaultWidth;
    height = DefaultHeight;

    windowedInfo = { .width = DefaultWidth, .height = DefaultHeight };
    glfwGetWindowPos(window, &windowedInfo.x, &windowedInfo.y);

    static std::function<void(GLFWwindow*, int, int)> ResizeCallback = [this](auto* w, auto width, auto height)
    { this->OnResize(w, width, height); };

    // Set static callback that uses the function object
    glfwSetWindowSizeCallback(window, [](GLFWwindow* w, int width, int height) { ResizeCallback(w, width, height); });

    static std::function<void(GLFWwindow*, int, int, int, int)> ToggleFullscreenCallback =
        [this](auto* w, auto key, auto scancode, auto action, auto mods)
    {
        // Alt+Enter to toggle fullscreen
        if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && (mods & GLFW_MOD_ALT))
        {
            this->ToggleFullscreen();
        }
    };

    glfwSetKeyCallback(window,
                       [](GLFWwindow* w, int key, int scancode, int action, int mods)
                       { ToggleFullscreenCallback(w, key, scancode, action, mods); });
}

ExampleApplication::~ExampleApplication()
{
    glfwSetWindowSizeCallback(window, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
}

void ExampleApplication::OnResize(GLFWwindow* window, uint32_t newWidth, uint32_t newHeight)
{
    width = newWidth;
    height = newHeight;
    if (graphics)
    {
        graphics->OnWindowResized(newWidth, newHeight);
    }
}

void ExampleApplication::ToggleFullscreen()
{
    if (windowMode == Windowed)
    {
        // Store current window position and size for restoration later.
        glfwGetWindowPos(window, &windowedInfo.x, &windowedInfo.y);
        glfwGetWindowSize(window, &windowedInfo.width, &windowedInfo.height);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

        windowMode = Fullscreen;
    }
    else // if (windowMode == Fullscreen)
    {
        // Use stored windowedInfo to return to windowed size.
        glfwSetWindowMonitor(window,
                             nullptr,
                             windowedInfo.x,
                             windowedInfo.y,
                             windowedInfo.width,
                             windowedInfo.height,
                             0);
        windowMode = Windowed;
    }

    // TODO: Force a resize?
}
