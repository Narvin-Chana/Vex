#pragma once

#include <Vex/NonNullPtr.h>

#include <RHI/RHICommandList.h>

#include <Vulkan/VkHeaders.h>

namespace vex
{
struct RHIDrawResources;
}

namespace vex::vk
{
struct VkGPUContext;

class VkTexture;

class VkCommandList final : public RHICommandListBase
{
public:
    VkCommandList(NonNullPtr<VkGPUContext> ctx, ::vk::UniqueCommandBuffer&& commandBuffer, QueueType type);

    virtual void Open() override;
    virtual void Close() override;

    virtual void SetViewport(
        float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) override;
    virtual void SetScissor(i32 x, i32 y, u32 width, u32 height) override;

    virtual void SetPipelineState(const RHIGraphicsPipelineState& graphicsPipelineState) override;
    virtual void SetPipelineState(const RHIComputePipelineState& computePipelineState) override;
    virtual void SetPipelineState(const RHIRayTracingPipelineState& rayTracingPipelineState) override;

    virtual void SetLayout(RHIResourceLayout& layout) override;
    virtual void SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout) override;
    virtual void SetInputAssembly(InputAssembly inputAssembly) override;
    virtual void ClearTexture(const RHITextureBinding& binding,
                              TextureUsage::Type usage,
                              const TextureClearValue& clearValue) override;

    virtual void Barrier(std::span<const RHIBufferBarrier> bufferBarriers,
                         std::span<const RHITextureBarrier> textureBarriers) override;

    virtual void BeginRendering(const RHIDrawResources& resources) override;
    virtual void EndRendering() override;

    virtual void Draw(u32 vertexCount, u32 instanceCount = 1, u32 vertexOffset = 0, u32 instanceOffset = 0) override;
    virtual void DrawIndexed(
        u32 indexCount, u32 instanceCount, u32 indexOffset, u32 vertexOffset, u32 instanceOffset) override;

    virtual void SetVertexBuffers(u32 startSlot, std::span<RHIBufferBinding> vertexBuffers) override;
    virtual void SetIndexBuffer(const RHIBufferBinding& indexBuffer) override;

    virtual void Dispatch(const std::array<u32, 3>& groupCount) override;

    virtual void TraceRays(const std::array<u32, 3>& widthHeightDepth,
                           const RHIRayTracingPipelineState& rayTracingPipelineState) override;

    virtual void GenerateMips(RHITexture& texture, u16 sourceMip) override;

    using RHICommandListBase::Copy;
    virtual void Copy(RHITexture& src,
                      RHITexture& dst,
                      std::span<const TextureCopyDesc> textureCopyDesc) override;
    virtual void Copy(RHIBuffer& src, RHIBuffer& dst, const BufferCopyDesc& bufferCopyDesc) override;
    virtual void Copy(RHIBuffer& src,
                      RHITexture& dst,
                      std::span<const BufferTextureCopyDesc> copyDesc) override;
    virtual void Copy(RHITexture& src,
                      RHIBuffer& dst,
                      std::span<const BufferTextureCopyDesc> copyDesc) override;

    ::vk::CommandBuffer GetNativeCommandList()
    {
        return *commandBuffer;
    }

    virtual RHIScopedGPUEvent CreateScopedMarker(const char* label, std::array<float, 3> labelColor) override;

private:
    NonNullPtr<VkGPUContext> ctx;
    ::vk::UniqueCommandBuffer commandBuffer;

    bool isRendering = false;

    std::optional<::vk::Viewport> cachedViewport{};
    std::optional<::vk::Rect2D> cachedScissor{};

    friend class VkRHI;
};

} // namespace vex::vk