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
    vex::PlatformWindowHandle platformWindow = glfwGetWin32Window(window);
#elif defined(__linux__)
    vex::PlatformWindowHandle platformWindow{
        .window = glfwGetX11Window(window),
        .display = glfwGetX11Display()
    };
#endif

    vex::UniqueHandle<vex::RHI> rhi{};
#define FORCE_VULKAN 1
#if VEX_VULKAN or FORCE_VULKAN
    vex::vk::RHICreateInfo createInfo;
    vex::u32 count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    createInfo.additionalExtensions.reserve(createInfo.additionalExtensions.size() + count);
    std::copy(extensions, extensions + count, std::back_inserter(createInfo.additionalExtensions));
    rhi = vex::MakeUnique<vex::vk::VkRHI>(createInfo);
#endif
#if VEX_DX12 or not FORCE_VULKAN
    rhi = vex::MakeUnique<vex::dx12::DX12RHI>();
#endif

    graphics = MakeUnique<vex::GfxBackend>(
        std::move(rhi),
        vex::BackendDescription{
            .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
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
