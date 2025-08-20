#pragma once

#include <RHI/RHIAllocator.h>

namespace vex::vk
{

class VkAllocator : public RHIAllocatorBase
{
public:
    VkAllocator() = default;

protected:
    virtual void OnPageAllocated(PageHandle handle, HeapType heapType) override;
    virtual void OnPageFreed(PageHandle handle, HeapType heapType) override;
};

} // namespace vex::vk