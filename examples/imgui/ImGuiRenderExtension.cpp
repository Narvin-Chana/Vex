#include "ImGuiRenderExtension.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#if VEX_VULKAN
#include <imgui_impl_vulkan.h>
#elif VEX_DX12
#include <imgui_impl_dx12.h>
#endif

#include <GLFW/glfw3.h> // for GLFWwindow

#include <Vex.h>
#include <Vex/Formats.h>
#include <Vex/FrameResource.h>
#include <Vex/GfxBackend.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>
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

#if VEX_VULKAN
    VEX_NOT_YET_IMPLEMENTED();
#elif VEX_DX12
    ImGui_ImplGlfw_InitForOther(window, true);

    static struct DescriptorHelper
    {
        std::unordered_map<size_t, vex::dx12::BindlessHandle> descriptorsMap;
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
        vex::dx12::BindlessHandle handle = helper.descriptorPool.AllocateStaticDescriptor();
        *cpuHandle = helper.descriptorPool.GetCPUDescriptor(handle);
        *gpuHandle = helper.descriptorPool.GetGPUDescriptor(handle);
        helper.descriptorsMap[cpuHandle->ptr] = handle;
    };
    initInfo.SrvDescriptorFreeFn =
        [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
    {
        vex::dx12::BindlessHandle handle = helper.descriptorsMap[cpuHandle.ptr];
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
    VEX_NOT_YET_IMPLEMENTED();
#elif VEX_DX12
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
#endif
}

void ImGuiRenderExtension::OnFrameEnd()
{
    // Call all user imgui calls
    ImGui::ShowDemoWindow();

    // Render ImGui to the backbuffer.
    {
        vex::CommandContext ctx = graphics.BeginScopedCommandContext(vex::CommandQueueType::Graphics);

        vex::ResourceBinding backBufferBinding = { .texture = graphics.GetCurrentBackBuffer() };

        vex::TextureClearValue clearValue{ .flags = vex::TextureClear::ClearColor, .color = { 0, 0, 0, 0 } };
        ctx.ClearTexture(backBufferBinding, &clearValue);

        // ImGui renders to the texture that is currently set as render target.
        // We have to manually set the render target we want ImGui to render to (in this case we want to render
        // directly to the backbuffer).
        ctx.SetRenderTarget(backBufferBinding);

#if VEX_VULKAN
        VEX_NOT_YET_IMPLEMENTED();
#elif VEX_DX12
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), ctx.GetRHICommandList().GetNativeCommandList().Get());
#endif
    }
}
