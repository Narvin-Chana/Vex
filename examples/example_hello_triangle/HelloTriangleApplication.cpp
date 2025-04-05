#include <HelloTriangleApplication.h>

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif

#if VEX_DX12
#include "DX12/DX12RHI.h"
#endif

#if VEX_VULKAN
#include "Vulkan/VkRHI.h"
#endif

#include <GLFW/glfw3native.h>

#include <Vex.h>

HelloTriangleApplication::HelloTriangleApplication()
{
#if defined(__linux__)
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    static constexpr uint32_t DefaultWidth = 1280, DefaultHeight = 600;
    window = glfwCreateWindow(DefaultWidth, DefaultHeight, "HelloTriangle", nullptr, nullptr);

#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)
    vex::PlatformWindowHandle platformWindow{ .window = glfwGetX11Window(window), .display = glfwGetX11Display() };
#endif

#define FORCE_VULKAN 1

    graphics = CreateGraphicsBackend(
#if VEX_VULKAN and FORCE_VULKAN
        vex::GraphicsAPI::Vulkan,
#else // VEX_DX12 and not FORCE_VULKAN
        vex::GraphicsAPI::DirectX12,
#endif
        vex::BackendDescription{
            .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
            .swapChainFormat = vex::TextureFormat::RGBA8_UNORM,
            .enableGPUDebugLayer = !VEX_SHIPPING,
            .enableGPUBasedValidation = !VEX_SHIPPING });
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
