#pragma once

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

class VkCommandList final : public RHICommandListInterface
{
public:
    VkCommandList(VkGPUContext& ctx, ::vk::UniqueCommandBuffer&& commandBuffer, CommandQueueType type);

    virtual bool IsOpen() const override;

    virtual void Open() override;
    virtual void Close() override;

    virtual void SetViewport(
        float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) override;
    virtual void SetScissor(i32 x, i32 y, u32 width, u32 height) override;

    virtual void SetPipelineState(const RHIGraphicsPipelineState& graphicsPipelineState) override;
    virtual void SetPipelineState(const RHIComputePipelineState& computePipelineState) override;

    virtual void SetLayout(RHIResourceLayout& layout) override;
    virtual void SetLayoutLocalConstants(const RHIResourceLayout& layout,
                                         std::span<const ConstantBinding> constants) override;
    virtual void SetLayoutResources(const RHIResourceLayout& layout,
                                    std::span<RHITextureBinding> textures,
                                    std::span<RHIBufferBinding> buffers,
                                    RHIDescriptorPool& descriptorPool) override;
    virtual void SetDescriptorPool(RHIDescriptorPool& descriptorPool, RHIResourceLayout& resourceLayout) override;
    virtual void SetInputAssembly(InputAssembly inputAssembly) override;
    virtual void ClearTexture(RHITexture& rhiTexture,
                              const ResourceBinding& clearBinding,
                              const TextureClearValue& clearValue) override;

    virtual void Transition(RHITexture& texture, RHITextureState::Flags newState) override;
    virtual void Transition(RHIBuffer& texture, RHIBufferState::Flags newState) override;
    virtual void Transition(std::span<std::pair<RHITexture&, RHITextureState::Flags>> textureNewStatePairs) override;
    virtual void Transition(std::span<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferNewStatePairs) override;

    virtual void BeginRendering(const RHIDrawResources& resources) override;
    virtual void EndRendering() override;

    virtual void Draw(u32 vertexCount) override;

    virtual void Dispatch(const std::array<u32, 3>& groupCount) override;

    virtual void Copy(RHITexture& src, RHITexture& dst) override;
    virtual void Copy(RHIBuffer& src, RHIBuffer& dst) override;

    virtual CommandQueueType GetType() const override;

private:
    VkGPUContext& ctx;
    ::vk::UniqueCommandBuffer commandBuffer;
    CommandQueueType type;
    bool isOpen = false;

    bool isRendering = false;

    std::optional<::vk::Viewport> cachedViewport{};
    std::optional<::vk::Rect2D> cachedScissor{};

    friend class VkRHI;
};

} // namespace vex::vk