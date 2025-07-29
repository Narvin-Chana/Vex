#include "ImGuiApplication.h"

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#if VEX_VULKAN
#include <imgui_impl_vulkan.h>
#elif VEX_DX12
#include <imgui_impl_dx12.h>
#endif

#if VEX_VULKAN
#include <Vulkan/VkRHIAccessor.h>
#elif VEX_DX12
#include <DX12/DX12CommandList.h>
#include <DX12/DX12DescriptorPool.h>
#include <DX12/DX12Formats.h>
#include <DX12/DX12RHIAccessor.h>
#endif

ImGuiApplication::ImGuiApplication()
    : ExampleApplication("ImGuiApplication")
{
#if defined(_WIN32)
    vex::PlatformWindowHandle platformWindow = { .window = glfwGetWin32Window(window) };
#elif defined(__linux__)
    vex::PlatformWindowHandle platformWindow{ .window = glfwGetX11Window(window), .display = glfwGetX11Display() };
#endif

    graphics = CreateGraphicsBackend(vex::BackendDescription{
        .platformWindow = { .windowHandle = platformWindow, .width = DefaultWidth, .height = DefaultHeight },
        .swapChainFormat = SwapchainFormat,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING });

    rhiAccessor = graphics->CreateRHIAccessor();

    SetupImGui();
}

ImGuiApplication::~ImGuiApplication()
{
#if VEX_VULKAN
    ImGui_ImplVulkan_Shutdown();
#elif VEX_DX12
    ImGui_ImplDX12_Shutdown();
#endif
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiApplication::HandleKeyInput(int key, int scancode, int action, int mods)
{
}

void ImGuiApplication::Run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        graphics->StartFrame();

        {
            vex::CommandContext ctx = graphics->BeginScopedCommandContext(vex::CommandQueueType::Graphics);

            vex::ResourceBinding backBufferBinding = { .texture = graphics->GetCurrentBackBuffer() };

            vex::TextureClearValue clearValue{ .flags = vex::TextureClear::ClearColor, .color = { 0, 0, 0, 0 } };
            ctx.ClearTexture(backBufferBinding, &clearValue);

            // We have to manually set the render target we want ImGui to render to (in this case we want to render
            // directly to the backbuffer).
            ctx.SetRenderTarget(backBufferBinding);

            // ImGui renders to the texture that is currently set as render target.
#if VEX_VULKAN
            VEX_NOT_YET_IMPLEMENTED();
#elif VEX_DX12
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::ShowDemoWindow();
            ImGui::Render();
            ImGui_ImplDX12_RenderDrawData(
                ImGui::GetDrawData(),
                reinterpret_cast<vex::dx12::DX12CommandList*>(ctx.GetRHICommandList())->GetNativeCommandList());
#endif
        }

        graphics->EndFrame(windowMode == Fullscreen);
    }
}

void ImGuiApplication::SetupImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

#if VEX_VULKAN
    VEX_NOT_YET_IMPLEMENTED();
#elif VEX_DX12
    vex::dx12::DX12RHIAccessor* dx12Accessor = reinterpret_cast<vex::dx12::DX12RHIAccessor*>(rhiAccessor.get());

    ImGui_ImplGlfw_InitForOther(window, true);

    static struct DescriptorHelper
    {
        std::unordered_map<size_t, vex::dx12::BindlessHandle> descriptorsMap;
        vex::dx12::DX12DescriptorPool* descriptorPool;
    } helper{ .descriptorPool = dx12Accessor->GetDescriptorPool() };

    ImGui_ImplDX12_InitInfo initInfo;
    initInfo.CommandQueue = dx12Accessor->GetNativeCommandQueue();
    initInfo.Device = dx12Accessor->GetNativeDevice();
    initInfo.NumFramesInFlight = std::to_underlying(FrameBuffering);
    initInfo.RTVFormat = vex::dx12::TextureFormatToDXGI(SwapchainFormat);

    // Descriptors callbacks to register and unregister handles.
    initInfo.SrvDescriptorHeap = dx12Accessor->GetNativeDescriptorHeap();
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* initInfo,
                                       D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
                                       D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle)
    {
        vex::dx12::BindlessHandle handle = helper.descriptorPool->AllocateStaticDescriptor();
        *cpuHandle = helper.descriptorPool->GetCPUDescriptor(handle);
        *gpuHandle = helper.descriptorPool->GetGPUDescriptor(handle);
        helper.descriptorsMap[cpuHandle->ptr] = handle;
    };
    initInfo.SrvDescriptorFreeFn =
        [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
    {
        vex::dx12::BindlessHandle handle = helper.descriptorsMap[cpuHandle.ptr];
        helper.descriptorPool->FreeStaticDescriptor(handle);
        helper.descriptorsMap.erase(cpuHandle.ptr);
    };

    ImGui_ImplDX12_Init(&initInfo);
#endif
}

void ImGuiApplication::OnResize(GLFWwindow* window, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    ExampleApplication::OnResize(window, width, height);
}
