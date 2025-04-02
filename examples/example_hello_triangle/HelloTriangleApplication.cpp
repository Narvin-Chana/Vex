#include <HelloTriangleApplication.h>

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include "DX12/DX12RHI.h"
#include "DX12/DX12Types.h"
#include "Vulkan/VkRHI.h"

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

    vex::UniqueHandle<vex::RHI> rhi{};
#if 0
    vex::vk::RHICreateInfo createInfo;
    vex::u32 count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    createInfo.additionnalExtensions.reserve(createInfo.additionnalExtensions.size() + count);
    std::copy(extensions, extensions + count, std::back_inserter(createInfo.additionnalExtensions));
    rhi = vex::MakeUnique<vex::vk::VkRHI>(createInfo);
#else
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
