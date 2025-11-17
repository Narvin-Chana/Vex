#pragma once

#include <Vex.h>
#include <Vex/RHIImpl/RHICommandList.h>

#if VEX_VULKAN
#include <Vulkan/VkFormats.h>
#elif VEX_DX12
#include <DX12/DX12Formats.h>
#endif

#if VEX_VULKAN
#include <imgui_impl_vulkan.h>
#elif VEX_DX12
#include <imgui_impl_dx12.h>
#endif

struct ImGui_ImplVex_InitInfo
{
    vex::NonNullPtr<vex::RHI> rhi;
    vex::NonNullPtr<vex::RHIDescriptorPool> descriptorPool;
    vex::FrameBuffering buffering;
    vex::TextureFormat swapchainFormat;
    bool isSwapChainSRGB = false;
    vex::TextureFormat depthStencilFormat = vex::TextureFormat::UNKNOWN;
};

inline void ImGui_ImplVex_Init(ImGui_ImplVex_InitInfo& data)
{
#if VEX_VULKAN
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Device = data.rhi->GetNativeDevice();
    initInfo.Instance = data.rhi->GetNativeInstance();
    initInfo.PhysicalDevice = data.rhi->GetNativePhysicalDevice();

    const auto& commandQueue = data.rhi->GetCommandQueue(vex::QueueType::Graphics);
    initInfo.Queue = commandQueue.queue;
    initInfo.QueueFamily = commandQueue.family;
    initInfo.ImageCount = std::to_underlying(data.buffering);
    initInfo.MinImageCount = initInfo.ImageCount;
    initInfo.DescriptorPool = data.descriptorPool->GetNativeDescriptorPool();
    initInfo.PipelineCache = data.rhi->GetNativePSOCache();

    initInfo.UseDynamicRendering = true;
    ::vk::Format colorAttachmentFormat = vex::vk::TextureFormatToVulkan(data.swapchainFormat, data.isSwapChainSRGB);
    ::vk::Format depthStencilFormat = vex::vk::TextureFormatToVulkan(data.depthStencilFormat, false);
    initInfo.PipelineRenderingCreateInfo =
        ::vk::PipelineRenderingCreateInfo{ .colorAttachmentCount = 1,
                                           .pColorAttachmentFormats = &colorAttachmentFormat,
                                           .depthAttachmentFormat = depthStencilFormat,
                                           .stencilAttachmentFormat = depthStencilFormat };

    ImGui_ImplVulkan_Init(&initInfo);
#elif VEX_DX12
    static struct DescriptorHelper
    {
        std::unordered_map<size_t, vex::BindlessHandle> descriptorsMap;
        vex::RHIDescriptorPool& descriptorPool;
    } helper{ .descriptorPool = *data.descriptorPool };

    ImGui_ImplDX12_InitInfo initInfo;
    initInfo.Device = data.rhi->GetNativeDevice().Get();
    initInfo.CommandQueue = data.rhi->GetNativeQueue(vex::QueueType::Graphics).Get();
    initInfo.NumFramesInFlight = std::to_underlying(data.buffering);
    initInfo.RTVFormat = vex::dx12::TextureFormatToDXGI(data.swapchainFormat, data.isSwapChainSRGB);
    initInfo.DSVFormat = vex::dx12::TextureFormatToDXGI(data.depthStencilFormat, false);

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

inline void ImGui_ImplVex_Shutdown()
{
#if VEX_VULKAN
    ImGui_ImplVulkan_Shutdown();
#elif VEX_DX12
    ImGui_ImplDX12_Shutdown();
#endif
}

inline void ImGui_ImplVex_RenderDrawData(vex::CommandContext& ctx)
{
#if VEX_VULKAN
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ctx.GetRHICommandList().GetNativeCommandList());
#elif VEX_DX12
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), ctx.GetRHICommandList().GetNativeCommandList().Get());
#endif
}

inline void ImGui_ImplVex_NewFrame()
{
#if VEX_VULKAN
    ImGui_ImplVulkan_NewFrame();
#elif VEX_DX12
    ImGui_ImplDX12_NewFrame();
#endif
}
