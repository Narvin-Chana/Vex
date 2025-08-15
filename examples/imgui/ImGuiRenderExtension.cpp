#include "ImGuiRenderExtension.h"

#if VEX_VULKAN
// ImGui uses vulkan.h headers, Vex uses vulkan.hpp,
// vulkan.hpp only compiles correctly when included BEFORE vulkan.h.
#include <Vulkan/VkHeaders.h>
#endif

#include <imgui.h>
#include <imgui_impl_glfw.h>

#if VEX_VULKAN
#include <imgui_impl_vulkan.h>
#elif VEX_DX12
#include <imgui_impl_dx12.h>
#endif

#include <Vex/CommandContext.h>
#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/GfxBackend.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>

#include "Vex/Bindings.h"
#if VEX_DX12
#include <DX12/DX12Formats.h>
#endif

ImGuiRenderExtension::ImGuiRenderExtension(vex::GfxBackend& graphics,
                                           GLFWwindow* window,
                                           vex::FrameBuffering buffering,
                                           vex::TextureFormat swapchainFormat)
    : graphics(graphics)
    , window(window)
    , buffering(buffering)
    , swapchainFormat(swapchainFormat)
{
}

ImGuiRenderExtension::~ImGuiRenderExtension()
{
}

void ImGuiRenderExtension::Initialize()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForOther(window, true);

#if VEX_VULKAN
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Device = data.rhi->GetNativeDevice();
    initInfo.Instance = data.rhi->GetNativeInstance();
    initInfo.PhysicalDevice = data.rhi->GetNativePhysicalDevice();

    const auto& commandQueue = data.rhi->GetCommandQueue(vex::CommandQueueType::Graphics);
    initInfo.Queue = commandQueue.queue;
    initInfo.QueueFamily = commandQueue.family;
    initInfo.ImageCount = std::to_underlying(buffering);
    initInfo.MinImageCount = initInfo.ImageCount;

    initInfo.DescriptorPool = data.descriptorPool->GetNativeDescriptorPool();
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&initInfo);
#elif VEX_DX12
    static struct DescriptorHelper
    {
        std::unordered_map<size_t, vex::BindlessHandle> descriptorsMap;
        vex::RHIDescriptorPool& descriptorPool;
    } helper{ .descriptorPool = *data.descriptorPool };

    ImGui_ImplDX12_InitInfo initInfo;
    initInfo.Device = data.rhi->GetNativeDevice().Get();
    initInfo.CommandQueue = data.rhi->GetNativeQueue(vex::CommandQueueType::Graphics).Get();
    initInfo.NumFramesInFlight = std::to_underlying(buffering);
    initInfo.RTVFormat = vex::dx12::TextureFormatToDXGI(swapchainFormat);

    // Descriptors callbacks to register and unregister handles.
    initInfo.SrvDescriptorHeap = helper.descriptorPool.GetNativeDescriptorHeap().Get();
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* initInfo,
                                       D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
                                       D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle)
    {
        vex::BindlessHandle handle = helper.descriptorPool.AllocateStaticDescriptor();
        *cpuHandle = helper.descriptorPool.GetCPUDescriptor(handle);
        *gpuHandle = helper.descriptorPool.GetGPUDescriptor(handle);
        helper.descriptorsMap[cpuHandle->ptr] = handle;
    };
    initInfo.SrvDescriptorFreeFn =
        [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
    {
        vex::BindlessHandle handle = helper.descriptorsMap[cpuHandle.ptr];
        helper.descriptorPool.FreeStaticDescriptor(handle);
        helper.descriptorsMap.erase(cpuHandle.ptr);
    };

    ImGui_ImplDX12_Init(&initInfo);
#endif
}

void ImGuiRenderExtension::Destroy()
{
#if VEX_VULKAN
    ImGui_ImplVulkan_Shutdown();
#elif VEX_DX12
    ImGui_ImplDX12_Shutdown();
#endif
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiRenderExtension::OnFrameStart()
{
#if VEX_VULKAN
    ImGui_ImplVulkan_NewFrame();
#elif VEX_DX12
    ImGui_ImplDX12_NewFrame();
#endif
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiRenderExtension::OnFrameEnd()
{
    // Call all user imgui calls
    ImGui::ShowDemoWindow();

    // Render ImGui to the backbuffer.
    {
        vex::CommandContext ctx = graphics.BeginScopedCommandContext(vex::CommandQueueType::Graphics);

        vex::TextureBinding backBufferBinding = { .texture = graphics.GetCurrentBackBuffer() };

        vex::TextureClearValue clearValue{ .flags = vex::TextureClear::ClearColor, .color = { 0, 0, 0, 0 } };
        ctx.ClearTexture(backBufferBinding, vex::TextureUsage::RenderTarget, &clearValue);

        // ImGui renders to the texture that is currently set as render target.
        // We have to manually set the render target we want ImGui to render to (in this case we want to render
        // directly to the backbuffer).
        ctx.BeginRendering({ .renderTargets = std::initializer_list{ backBufferBinding } });

        ImGui::Render();
#if VEX_VULKAN
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ctx.GetRHICommandList().GetNativeCommandList());
#elif VEX_DX12
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), ctx.GetRHICommandList().GetNativeCommandList().Get());
#endif

        ctx.EndRendering();
    }
}
