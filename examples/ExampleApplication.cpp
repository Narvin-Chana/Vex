#include "ExampleApplication.h"

#include <GLFWIncludes.h>

// Native GLFW access must stay the last include: the platform headers it pulls in (X11 especially) define macros such
// as None, Bool or Status that would break any Vex header included after it.
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
// When Vex is included textually, its Vulkan headers have already included Xlib.h and then undefined some of its macros.
// Xlib's include guard prevents them from coming back, yet the X11 headers glfw3native.h includes still rely on them, so
// restore Xlib's own definitions.
#ifndef Bool
#define Bool int
#endif
#ifndef Status
#define Status int
#endif
#ifndef True
#define True 1
#endif
#ifndef False
#define False 0
#endif
#endif
#include <GLFW/glfw3native.h>

ExampleApplication::ExampleApplication(std::string_view windowName,
                                       int defaultWidth,
                                       int defaultHeight,
                                       bool allowResize)
{
    SetupShaderErrorHandling();

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
        shaderCompiler.RecompileChangedShaders();
    }
}

void ExampleApplication::OnResize(GLFWwindow* window, int newWidth, int newHeight)
{
    width = newWidth;
    height = newHeight;
    if (graphics)
    {
        graphics->OnWindowResized(static_cast<unsigned int>(newWidth), static_cast<unsigned int>(newHeight));
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
    // Suggestion of an intrusive (a la Unreal) way to display errors.
    // The handling of shader compilation errors is user choice.
    shaderCompiler.SetShaderCompilationErrorsCallback(
        [](vex::Span<const std::pair<vex::ShaderKey, std::string>> errors) -> vex::ShaderHotReloadErrorResponse
        {
            std::string totalErrorMessage = "Error compiling shader(s):\n";
            for (auto& [key, err] : errors)
            {
                totalErrorMessage.append(std::format("Shader: {} - Error: {}\n", key, err));
            }
            totalErrorMessage.append("\nDo you want to retry?");

            const vex::i32 result = MessageBox(NULL,
                                               totalErrorMessage.c_str(),
                                               "Shader Compilation Error",
                                               MB_ICONERROR | MB_ABORTRETRYIGNORE | MB_DEFBUTTON2);
            if (result == IDRETRY)
            {
                return vex::ShaderHotReloadErrorResponse::RetryAndRetainPreviousShader;
            }
            if (result == IDIGNORE)
            {
                return vex::ShaderHotReloadErrorResponse::RetainPreviousShader;
            }
            if (result == IDABORT)
            {
                return vex::ShaderHotReloadErrorResponse::Abort;
            }
            return vex::ShaderHotReloadErrorResponse::Abort;
        });
#endif
}

vex::PlatformWindowHandle ExampleApplication::GetPlatformWindowHandle() const
{
#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { vex::PlatformWindowHandle::WindowsHandle{
        .window = glfwGetWin32Window(window) } };
#elif defined(__linux__)

    // Ask GLFW which backend it picked instead of probing: the native getters of an inactive backend raise errors.
    vex::PlatformWindowHandle platformWindow;
    switch (glfwGetPlatform())
    {
    case GLFW_PLATFORM_X11:
        platformWindow = { vex::PlatformWindowHandle::X11Handle{ .window = glfwGetX11Window(window),
                                                                 .display = glfwGetX11Display() } };
        break;
    case GLFW_PLATFORM_WAYLAND:
        platformWindow = { vex::PlatformWindowHandle::WaylandHandle{ .window = glfwGetWaylandWindow(window),
                                                                     .display = glfwGetWaylandDisplay() } };
        break;
    default:
        VEX_LOG(vex::Fatal, "Unsupported GLFW platform, only X11 and Wayland are supported on Linux.");
        break;
    }
#endif

    return platformWindow;
}
