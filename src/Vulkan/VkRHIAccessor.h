#pragma once

#include <Vex/RHI/RHIAccessor.h>

namespace vex::vk
{

class VkRHIAccessor : public RHIAccessor
{
public:
    virtual ~VkRHIAccessor() override;
    void* GetNativeDevice();
    void* GetNativeDescriptorPool();
};

} // namespace vex::vk