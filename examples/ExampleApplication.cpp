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

ExampleApplication::ExampleApplication(std::string_view windowName)
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

    window = glfwCreateWindow(DefaultWidth, DefaultHeight, std::string(windowName).c_str(), nullptr, nullptr);

    width = DefaultWidth;
    height = DefaultHeight;

    windowedInfo = { .width = DefaultWidth, .height = DefaultHeight };
    glfwGetWindowPos(window, &windowedInfo.x, &windowedInfo.y);

    // Store the 'this' pointer for callbacks
    glfwSetWindowUserPointer(window, this);

    glfwSetWindowSizeCallback(window,
                              [](GLFWwindow* w, int width, int height)
                              {
                                  auto* self = static_cast<ExampleApplication*>(glfwGetWindowUserPointer(w));
                                  self->OnResize(w, width, height);
                              });

    glfwSetKeyCallback(window,
                       [](GLFWwindow* w, int key, int scancode, int action, int mods)
                       {
                           auto* self = static_cast<ExampleApplication*>(glfwGetWindowUserPointer(w));

                           // Alt+Enter to toggle fullscreen
                           if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && (mods & GLFW_MOD_ALT))
                           {
                               self->ToggleFullscreen();
                           }

                           self->HandleKeyInput(key, scancode, action, mods);
                       });
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
}
