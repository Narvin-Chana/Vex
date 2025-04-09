#pragma once
#include "Vex/RHI/RHICommandList.h"
#include "VkHeaders.h"

namespace vex::vk
{

class VkCommandList : public RHICommandList
{
public:
    virtual bool IsOpen() const;

    virtual void Open();
    virtual void Close();

    virtual void SetViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);
    virtual void SetScissor(i32 x, i32 y, u32 width, u32 height);

    virtual CommandQueueType GetType() const;

    VkCommandList(::vk::UniqueCommandBuffer&& commandBuffer, CommandQueueType type);

private:
    ::vk::UniqueCommandBuffer commandBuffer;
    CommandQueueType type;
    bool isOpen = false;

    friend class VkRHI;
};

} // namespace vex::vk