#pragma once

#include "VkHeaders.h"

#include <Vex/RHI/RHIBuffer.h>
#include <Vex/Types.h>

#include <span>

namespace vex
{
struct BufferDescription;
namespace BufferUtil
{
::vk::AccessFlags2 GetAccessFlagsFromBufferState(RHIBufferState::Flags flags);
}
} // namespace vex

namespace vex::vk
{
struct VkGPUContext;

class VkBuffer : public RHIBuffer
{
public:
    VkBuffer(VkGPUContext& ctx, const BufferDescription& desc);

    virtual bool CanBeMapped() override;
    virtual std::span<u8> Map() override;
    virtual void UnMap() override;

    ::vk::Buffer& GetBuffer();

private:
    ::vk::UniqueBuffer buffer;
    ::vk::UniqueBuffer stagingBuffer; // staging buffer if necessary
    ::vk::UniqueDeviceMemory memory;  // Memory size is doubled when we need a staging buffer
    bool needsStagingBuffer = false;
    VkGPUContext& ctx;
};

} // namespace vex::vk
