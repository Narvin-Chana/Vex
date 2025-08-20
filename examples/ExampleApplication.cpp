#include "ExampleApplication.h"

#include <functional>

#include <GLFWIncludes.h>

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

void ExampleApplication::HandleKeyInput(int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS && key == GLFW_KEY_R && graphics)
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
