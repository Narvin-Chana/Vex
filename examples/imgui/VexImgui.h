#pragma once

#include <Vex.h>
#include <Vex/RHIImpl/RHICommandList.h>

#if VEX_VULKAN
#include <Vulkan/VkErrorHandler.h>
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
    vex::TextureFormat depthStencilFormat = vex::TextureFormat::UNKNOWN;
};

#if VEX_VULKAN
struct ImGui_ImplVex_VulkanInfo
{
    ::vk::UniqueSampler linearSampler;
    std::unordered_map<VkImageView, ImTextureID> imageCache;
};
inline ImGui_ImplVex_VulkanInfo GVulkanInfo;
#endif

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
    ::vk::Format colorAttachmentFormat = vex::vk::TextureFormatToVulkan(data.swapchainFormat, false);
    ::vk::Format depthStencilFormat = vex::vk::TextureFormatToVulkan(data.depthStencilFormat, false);
    initInfo.PipelineRenderingCreateInfo =
        ::vk::PipelineRenderingCreateInfo{ .colorAttachmentCount = 1,
                                           .pColorAttachmentFormats = &colorAttachmentFormat,
                                           .depthAttachmentFormat = depthStencilFormat,
                                           .stencilAttachmentFormat = depthStencilFormat };

    ImGui_ImplVulkan_Init(&initInfo);

    ::vk::SamplerCreateInfo samplerCI{ .magFilter = ::vk::Filter::eLinear,
                                       .minFilter = ::vk::Filter::eLinear,
                                       .mipmapMode = ::vk::SamplerMipmapMode::eLinear,
                                       .addressModeU = ::vk::SamplerAddressMode::eClampToEdge,
                                       .addressModeV = ::vk::SamplerAddressMode::eClampToEdge,
                                       .addressModeW = ::vk::SamplerAddressMode::eClampToEdge,
                                       .maxAnisotropy = 1,
                                       .minLod = -1000,
                                       .maxLod = 1000 };

    GVulkanInfo.linearSampler = vex::vk::VEX_VK_CHECK <<= data.rhi->GetNativeDevice().createSamplerUnique(samplerCI);
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
    initInfo.RTVFormat = vex::dx12::TextureFormatToDXGI(data.swapchainFormat, false);
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
    GVulkanInfo = {};
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

namespace ImGui
{

inline void Image(vex::Graphics& gfx,
                  const vex::Texture& texture,
                  const ImVec2& image_size,
                  const ImVec2& uv0 = ImVec2(0, 0),
                  const ImVec2& uv1 = ImVec2(1, 1))
{
    vex::RHIAccessor accessor{ gfx };
    VEX_ASSERT(texture.handle != vex::GInvalidTextureHandle);
    ImTextureID registeredTexture;
#if VEX_VULKAN
    ::vk::ImageView img = accessor.GetTexture(texture).GetOrCreateImageView({ texture }, vex::TextureUsage::ShaderRead);
    if (!GVulkanInfo.imageCache.contains(img))
    {
        GVulkanInfo.imageCache.insert(
            { img,
              (ImTextureID)ImGui_ImplVulkan_AddTexture(*GVulkanInfo.linearSampler,
                                                       img,
                                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) });
    }
    registeredTexture = GVulkanInfo.imageCache[img];
#elif VEX_DX12
    vex::TextureBinding binding{ texture, vex::TextureBindingUsage::ShaderRead };
    vex::BindlessHandle handle = gfx.GetBindlessHandle(binding);
    CD3DX12_GPU_DESCRIPTOR_HANDLE descriptorHandle = accessor.GetDescriptorPool().GetGPUDescriptor(handle);
    registeredTexture = descriptorHandle.ptr;
#endif
    Image(registeredTexture, image_size, uv0, uv1);
}

} // namespace ImGui
