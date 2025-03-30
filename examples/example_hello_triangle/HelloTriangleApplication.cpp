#include <HelloTriangleApplication.h>

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>

#include <Vex.h>

HelloTriangleApplication::HelloTriangleApplication()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    static constexpr uint32_t DefaultWidth = 1280, DefaultHeight = 600;
    window = glfwCreateWindow(DefaultWidth, DefaultHeight, "HelloTriangle", nullptr, nullptr);

#if defined(_WIN32)
    HWND platformWindow = glfwGetWin32Window(window);
#elif defined(__linux__)
    // TODO: FIGURE OUT THE HANDLE TYPE ON LINUX
    int platformWindow = -1;
#endif
    graphics = vex::CreateGraphicsBackend(
        vex::GraphicsAPI::Vulkan,
        { .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
          .swapChainFormat = vex::TextureFormat::RGBA8_UNORM });
}

HelloTriangleApplication::~HelloTriangleApplication()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

void HelloTriangleApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }
}
