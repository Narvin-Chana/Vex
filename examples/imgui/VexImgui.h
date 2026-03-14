#pragma once

#include <Vex.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHITexture.h>

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
    vex::NonNullPtr<vex::Graphics> graphics;
    vex::FrameBuffering buffering;
    vex::TextureFormat swapchainFormat;
    vex::TextureFormat depthStencilFormat = vex::TextureFormat::UNKNOWN;
};

struct ImGui_ImplVex_Context
{
    vex::Graphics* graphics = nullptr;
#if VEX_VULKAN
    ::vk::UniqueSampler linearSampler;
    std::unordered_map<VkImageView, ImTextureID> imageCache;
#endif
};
inline ImGui_ImplVex_Context GImGuiVexContext;

inline void ImGui_ImplVex_Init(ImGui_ImplVex_InitInfo& data)
{
    const vex::RHIAccessor accessor{ *data.graphics };
    vex::NonNullPtr rhi = accessor.GetRHI();
    vex::NonNullPtr descriptorPool = accessor.GetDescriptorPool();

    GImGuiVexContext.graphics = data.graphics;
#if VEX_VULKAN
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Device = rhi->GetNativeDevice();
    initInfo.Instance = rhi->GetNativeInstance();
    initInfo.PhysicalDevice = rhi->GetNativePhysicalDevice();

    const auto& commandQueue = rhi->GetCommandQueue(vex::QueueType::Graphics);
    initInfo.Queue = commandQueue.queue;
    initInfo.QueueFamily = commandQueue.family;
    initInfo.ImageCount = std::to_underlying(data.buffering);
    initInfo.MinImageCount = initInfo.ImageCount;
    initInfo.DescriptorPool = descriptorPool->GetNativeDescriptorPool();
    initInfo.PipelineCache = rhi->GetNativePSOCache();

    initInfo.UseDynamicRendering = true;
    ::vk::Format colorAttachmentFormat = vex::vk::TextureFormatToVulkan(data.swapchainFormat, false);
    ::vk::Format depthStencilFormat = vex::vk::TextureFormatToVulkan(data.depthStencilFormat, false);
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo =
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

    GImGuiVexContext.linearSampler = vex::vk::VEX_VK_CHECK <<= rhi->GetNativeDevice().createSamplerUnique(samplerCI);
#elif VEX_DX12
    static struct DescriptorHelper
    {
        std::unordered_map<size_t, vex::BindlessHandle> descriptorsMap;
        vex::RHIDescriptorPool& descriptorPool;
    } helper{ .descriptorPool = *descriptorPool };

    ImGui_ImplDX12_InitInfo initInfo;
    initInfo.Device = rhi->GetNativeDevice().Get();
    initInfo.CommandQueue = rhi->GetNativeQueue(vex::QueueType::Graphics).Get();
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
    GImGuiVexContext = {};
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

inline void Image(const vex::TextureBinding& binding,
                  const ImVec2& image_size,
                  const ImVec2& uv0 = ImVec2(0, 0),
                  const ImVec2& uv1 = ImVec2(1, 1))
{
    VEX_CHECK(GImGuiVexContext.graphics, "ImGui_ImplVex_Init must be called prior to this");
    VEX_CHECK(binding.texture.handle != vex::GInvalidTextureHandle,
              "Vex Imgui: Texture handle need to be valid to draw");

    vex::RHIAccessor accessor{ *GImGuiVexContext.graphics };
    ImTextureID registeredTexture;
#if VEX_VULKAN
    ::vk::ImageView img =
        accessor.GetTexture(binding.texture).GetOrCreateImageView(binding, vex::TextureUsage::ShaderRead);
    if (!GImGuiVexContext.imageCache.contains(img))
    {
        GImGuiVexContext.imageCache.insert(
            { img,
              (ImTextureID)ImGui_ImplVulkan_AddTexture(*GImGuiVexContext.linearSampler,
                                                       img,
                                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) });
    }
    registeredTexture = GImGuiVexContext.imageCache[img];
#elif VEX_DX12
    vex::BindlessHandle handle = GImGuiVexContext.graphics->GetBindlessHandle(binding);
    CD3DX12_GPU_DESCRIPTOR_HANDLE descriptorHandle = accessor.GetDescriptorPool().GetGPUDescriptor(handle);
    registeredTexture = descriptorHandle.ptr;
#endif
    Image(registeredTexture, image_size, uv0, uv1);
}

inline void Image(const vex::Texture& texture,
                  const ImVec2& image_size,
                  const ImVec2& uv0 = ImVec2(0, 0),
                  const ImVec2& uv1 = ImVec2(1, 1))
{
    Image(vex::TextureBinding{ .texture = texture, .usage = vex::TextureBindingUsage::ShaderRead },
          image_size,
          uv0,
          uv1);
}

} // namespace ImGui
