﻿#pragma once

#include <span>

#include <Vex/NonNullPtr.h>
#include <Vex/Types.h>

#include <RHI/RHIAllocator.h>
#include <RHI/RHIBuffer.h>

#include <Vulkan/RHI/VkDescriptorPool.h>
#include <Vulkan/VkHeaders.h>

namespace vex
{
struct BufferDescription;
} // namespace vex

namespace vex::vk
{
struct VkStagingBuffer;
struct VkGPUContext;
struct VkDirectBufferMemory;

class VkBuffer final : public RHIBufferBase
{
public:
    VkBuffer(NonNullPtr<VkGPUContext> ctx, VkAllocator& allocator, const BufferDescription& desc);

    virtual BindlessHandle GetOrCreateBindlessView(BufferBindingUsage usage,
                                                   std::optional<u32> strideByteSize,
                                                   RHIDescriptorPool& descriptorPool) override;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;
    virtual void FreeAllocation(RHIAllocator& allocator) override;

    ::vk::Buffer GetNativeBuffer();

    virtual std::span<byte> Map() override;
    virtual void Unmap() override;

private:
    NonNullPtr<VkGPUContext> ctx;

    ::vk::UniqueBuffer buffer;

    std::optional<BindlessHandle> bufferHandle;

#if !VEX_USE_CUSTOM_ALLOCATOR_BUFFERS
    ::vk::UniqueDeviceMemory memory;
#endif

    friend struct VkStagedBufferMemory;
    friend struct VkDirectBufferMemory;
};

} // namespace vex::vk
