#include "ExampleApplication.h"

#include <functional>

#include <GLFWIncludes.h>

ExampleApplication::ExampleApplication(std::string_view windowName,
                                       int defaultWidth,
                                       int defaultHeight,
                                       bool allowResize)
{
    if (!glfwInit())
    {
        VEX_LOG(vex::Fatal, "Unable to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, allowResize ? GLFW_TRUE : GLFW_FALSE);

    width = defaultWidth ? defaultWidth : DefaultWidth;
    height = defaultHeight ? defaultHeight : DefaultHeight;

    window = glfwCreateWindow(width, height, std::string(windowName).c_str(), nullptr, nullptr);

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

    graphics.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
}

void ExampleApplication::HandleKeyInput(int key, int scancode, int action, int mods)
{
    // CTRL+SHIFT+PERIOD to recompile shaders.
    if (action == GLFW_PRESS && key == GLFW_KEY_PERIOD && graphics && (mods & GLFW_MOD_CONTROL))
    {
        graphics->RecompileChangedShaders();
    }
}

void ExampleApplication::OnResize(GLFWwindow* window, uint32_t newWidth, uint32_t newHeight)
{
    width = static_cast<int32_t>(newWidth);
    height = static_cast<int32_t>(newHeight);
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

void ExampleApplication::SetupShaderErrorHandling()
{
#if defined(_WIN32)
    VEX_ASSERT(graphics, "Graphics backend must be defined!");
    // Suggestion of an intrusive (a la Unreal) way to display errors.
    // The handling of shader compilation errors is user choice.
    graphics->SetShaderCompilationErrorsCallback(
        [window = window](const std::vector<std::pair<vex::ShaderKey, std::string>>& errors) -> bool
        {
            if (!errors.empty())
            {
                std::string totalErrorMessage = "Error compiling shader(s):\n";
                for (auto& [key, err] : errors)
                {
                    totalErrorMessage.append(std::format("Shader: {} - Error: {}\n", key, err));
                }
                totalErrorMessage.append("\nDo you want to retry?");

                vex::i32 result = MessageBox(NULL,
                                             totalErrorMessage.c_str(),
                                             "Shader Compilation Error",
                                             MB_ICONERROR | MB_YESNO | MB_DEFBUTTON2);
                if (result == IDYES)
                {
                    return true;
                }
                else if (result == IDNO)
                {
                    VEX_LOG(vex::Error, "Unable to continue with shader errors. Closing application.");
                    glfwSetWindowShouldClose(window, true);
                }
            }

            return false;
        });
#endif
}

vex::PlatformWindowHandle ExampleApplication::GetPlatformWindowHandle() const
{
#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)

    vex::PlatformWindowHandle platformWindow;
    if (auto x11Window =  glfwGetX11Window(window))
    {
        platformWindow = { vex::PlatformWindowHandle::X11Handle{.window = x11Window, .display = glfwGetX11Display()} };
    }
    else if (auto waylandWindow = glfwGetWaylandWindow(window))
    {
        platformWindow = { vex::PlatformWindowHandle::WaylandHandle{.window = waylandWindow, .display = glfwGetWaylandDisplay()} };
    }
#endif

    return platformWindow;
}
