#pragma once

#include <Vex/Utility/NonNullPtr.h>
#include <Vex/Types.h>

#include <RHI/RHIAllocator.h>
#include <RHI/RHIBuffer.h>

#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/VkHeaders.h>

namespace vex
{
struct BufferDesc;
} // namespace vex

namespace vex::vk
{
struct VkStagingBuffer;
struct VkGPUContext;
struct VkDirectBufferMemory;

class VkBuffer final : public RHIBufferBase
{
public:
    VkBuffer(NonNullPtr<VkGPUContext> ctx, VkAllocator& allocator, const BufferDesc& desc);

    void AllocateBindlessHandle(RHIDescriptorPool& descriptorPool,
                                BindlessHandle handle,
                                const BufferViewDesc& desc) override;
    ::vk::Buffer GetNativeBuffer();

    virtual Span<byte> Map() override;
    virtual void Unmap() override;

private:
    NonNullPtr<VkGPUContext> ctx;

    ::vk::UniqueBuffer buffer;

#if !VEX_USE_CUSTOM_ALLOCATOR_BUFFERS
    ::vk::UniqueDeviceMemory memory;
#endif

    friend struct VkStagedBufferMemory;
    friend struct VkDirectBufferMemory;
};

} // namespace vex::vk
