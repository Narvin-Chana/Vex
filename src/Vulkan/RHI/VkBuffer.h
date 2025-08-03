#pragma once

#include <span>

#include <Vex/Types.h>

#include <RHI/RHIBuffer.h>

#include <Vulkan/RHI/VkDescriptorPool.h>
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
struct VkDirectBufferMemory;

class VkBuffer final : public RHIBufferInterface
{
public:
    VkBuffer(VkGPUContext& ctx, const BufferDescription& desc);

    virtual BindlessHandle GetOrCreateBindlessView(BufferUsage::Type, RHIDescriptorPool& descriptorPool) override;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;

    ::vk::Buffer GetNativeBuffer();

    virtual std::span<u8> Map() override;
    virtual void Unmap() override;

private:
    UniqueHandle<VkBuffer> CreateStagingBuffer() override;

    ::vk::UniqueBuffer buffer;
    ::vk::UniqueDeviceMemory memory;
    VkGPUContext& ctx;

    std::optional<BindlessHandle> bufferHandle;

    friend struct VkStagedBufferMemory;
    friend struct VkDirectBufferMemory;
};

} // namespace vex::vk
