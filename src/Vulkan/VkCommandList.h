#pragma once

#include <Vex/RHI/RHICommandList.h>

#include "VkHeaders.h"

namespace vex::vk
{
struct VkGPUContext;

class VkTexture;

class VkCommandList : public RHICommandList
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
    virtual void Transition(std::span<std::pair<RHITexture&, RHITextureState::Flags>> textureNewStatePairs) override;

    virtual void Draw(u32 vertexCount) override;

    virtual void Dispatch(const std::array<u32, 3>& groupCount) override;

    virtual void Copy(RHITexture& src, RHITexture& dst) override;

    virtual CommandQueueType GetType() const override;

private:
    VkGPUContext& ctx;
    ::vk::UniqueCommandBuffer commandBuffer;
    CommandQueueType type;
    bool isOpen = false;

    friend class VkRHI;
};

} // namespace vex::vk