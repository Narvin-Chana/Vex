﻿#pragma once

#include <Vex/RHI/RHICommandList.h>

#include "VkHeaders.h"

namespace vex::vk
{

class VkCommandList : public RHICommandList
{
public:
    VkCommandList(::vk::UniqueCommandBuffer&& commandBuffer, CommandQueueType type);

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

    virtual void Dispatch(const std::array<u32, 3>& groupCount,
                          RHIResourceLayout& layout,
                          RHITexture& backbuffer) override;

    virtual CommandQueueType GetType() const override;

private:
    ::vk::UniqueCommandBuffer commandBuffer;
    CommandQueueType type;
    bool isOpen = false;

    friend class VkRHI;
};

} // namespace vex::vk