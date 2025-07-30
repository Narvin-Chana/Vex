#pragma once

#include <span>

#include <Vex/RHI/RHIBuffer.h>
#include <Vex/Types.h>

#include <Vulkan/VkDescriptorPool.h>
#include <Vulkan/VkHeaders.h>

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
struct VkStagingBuffer;
struct VkGPUContext;
class VkBuffer;
struct VkDirectBufferMemory;

class VkBuffer : public RHIBuffer
{
public:
    VkBuffer(VkGPUContext& ctx, const BufferDescription& desc);

    BindlessHandle GetOrCreateBindlessIndex(VkGPUContext& ctx, VkDescriptorPool& descriptorPool);
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;
    ::vk::Buffer& GetBuffer();

    virtual std::span<u8> Map() override;
    virtual void Unmap() override;

private:
    virtual UniqueHandle<RHIBuffer> CreateStagingBuffer() override;

    ::vk::UniqueBuffer buffer;
    ::vk::UniqueDeviceMemory memory;
    VkGPUContext& ctx;

    std::optional<BindlessHandle> bufferHandle;

    friend struct VkStagedBufferMemory;
    friend struct VkDirectBufferMemory;
};

} // namespace vex::vk
