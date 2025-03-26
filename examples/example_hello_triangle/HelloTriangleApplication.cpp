#include <HelloTriangleApplication.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <Vex.h>

HelloTriangleApplication::HelloTriangleApplication()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    static constexpr uint32_t DefaultWidth = 1280, DefaultHeight = 600;
    window = glfwCreateWindow(DefaultWidth, DefaultHeight, "HelloTriangle", nullptr, nullptr);

    HWND platformWindow = glfwGetWin32Window(window);
    graphics = vex::CreateGraphicsBackend(
        { .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
          .swapChainFormat = vex::Format::RGBA8_UNORM });
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
