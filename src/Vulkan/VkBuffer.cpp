#include "VkBuffer.h"

#include <Vex/Buffer.h>

#include "VkCommandQueue.h"
#include "VkErrorHandler.h"
#include "VkGPUContext.h"
#include "VkMemory.h"

namespace vex::vk
{
::vk::BufferUsageFlags GetVkBufferUsageFromDesc(const BufferDescription& desc)
{
    using enum ::vk::BufferUsageFlagBits;
    switch (desc.usage)
    {
    case BufferUsage::StagingBuffer:
        return eTransferSrc;
    case BufferUsage::GenericBuffer:
        return eStorageBuffer;
    case BufferUsage::IndexBuffer:
        return eIndexBuffer;
    case BufferUsage::VertexBuffer:
        return eVertexBuffer;
    default:
        VEX_LOG(Fatal, "RHIBuffer::Usage not supported by VulkanRHI")
        break;
    }
    std::unreachable();
}

::vk::MemoryPropertyFlags GetMemoryPropsFromDesc(const BufferDescription& desc)
{
    using enum ::vk::MemoryPropertyFlagBits;

    bool GPUWrite = desc.memoryAcces & BufferMemoryAccess::GPUWrite;
    bool GPURead = desc.memoryAcces & BufferMemoryAccess::GPURead;
    bool CPUWrite = desc.memoryAcces & BufferMemoryAccess::CPUWrite;
    bool CPURead = desc.memoryAcces & BufferMemoryAccess::CPURead;

    if (!CPUWrite && !CPURead)
    {
        if (GPUWrite || GPURead)
        {
            return eDeviceLocal;
        }
    }
    else
    {
        if (GPUWrite || GPURead)
        {
            return eHostCoherent | eHostVisible;
        }
    }

    std::unreachable();
}

VkDirectBufferMemory::VkDirectBufferMemory(VkGPUContext& ctx, ::vk::DeviceMemory memory, u32 size)
    : ctx{ ctx }
    , memory{ memory }
    , size{ size }
{
    data = static_cast<u8*>(VEX_VK_CHECK <<= ctx.device.mapMemory(memory, 0, size));
}

void VkDirectBufferMemory::SetData(std::span<const u8> inData)
{
    std::ranges::copy(inData, data);
}

VkDirectBufferMemory::~VkDirectBufferMemory()
{
    ctx.device.unmapMemory(memory);
}

VkStagedBufferMemory::VkStagedBufferMemory(VkGPUContext& ctx, VkBuffer& buffer)
    : buffer{ buffer }
    , mapping{ ctx, *buffer.stagingBuffer->memory, buffer.desc.size }
{
}

void VkStagedBufferMemory::SetData(std::span<const u8> data)
{
    mapping.SetData(data);
}

VkStagedBufferMemory::~VkStagedBufferMemory()
{
    buffer.SetNeedsStagingBufferCopy(true);
}

VkBuffer::VkBuffer(VkGPUContext& ctx, const BufferDescription& desc)
    : RHIBuffer{ desc }
    , ctx{ ctx }
{
    auto bufferUsage = GetVkBufferUsageFromDesc(desc);
    auto memoryProps = GetMemoryPropsFromDesc(desc);

    bool needsStagingBuffer = false;
    if (memoryProps & ::vk::MemoryPropertyFlagBits::eDeviceLocal)
    {
        // Needs to get its data from somewhere. Will therefore always need a transfer dest usage
        bufferUsage |= ::vk::BufferUsageFlagBits::eTransferDst;
        needsStagingBuffer = true;
    }

    buffer = VEX_VK_CHECK <<=
        ctx.device.createBufferUnique({ .size = desc.size,
                                        .usage = bufferUsage,
                                        .sharingMode = ::vk::SharingMode::eExclusive,
                                        .queueFamilyIndexCount = 1,
                                        .pQueueFamilyIndices = &ctx.graphicsPresentQueue.family });

    const ::vk::MemoryRequirements reqs = ctx.device.getBufferMemoryRequirements(*buffer);

    memory = VEX_VK_CHECK <<= ctx.device.allocateMemoryUnique(
        { .allocationSize = reqs.size,
          .memoryTypeIndex = GetBestMemoryType(ctx.physDevice, reqs.memoryTypeBits, memoryProps) });

    VEX_VK_CHECK << ctx.device.bindBufferMemory(*buffer, *memory, 0);

    if (needsStagingBuffer)
    {
        stagingBuffer =
            MakeUnique<VkBuffer>(ctx,
                                 BufferDescription{
                                     .name = desc.name + "_StagingBuffer",
                                     .size = desc.size,
                                     .usage = BufferUsage::StagingBuffer,
                                     .memoryAcces = BufferMemoryAccess::CPUWrite | BufferMemoryAccess::GPURead,
                                 });
    }
}

UniqueHandle<RHIMappedBufferMemory> VkBuffer::GetMappedMemory()
{
    if (stagingBuffer)
    {
        return MakeUnique<VkStagedBufferMemory>(ctx, *this);
    }
    return MakeUnique<VkDirectBufferMemory>(ctx, *memory, desc.size);
}

BindlessHandle VkBuffer::GetOrCreateBindlessIndex(VkGPUContext& ctx, VkDescriptorPool& descriptorPool)
{
    if (bufferHandle)
    {
        return *bufferHandle;
    }

    const BindlessHandle handle = descriptorPool.AllocateStaticDescriptor(*this);

    descriptorPool.UpdateDescriptor(ctx,
                                    handle,
                                    ::vk::DescriptorBufferInfo{ .buffer = *buffer, .offset = 0, .range = desc.size });

    bufferHandle = handle;

    return handle;
}

void VkBuffer::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    if (bufferHandle && *bufferHandle != GInvalidBindlessHandle)
    {
        reinterpret_cast<VkDescriptorPool&>(descriptorPool).FreeStaticDescriptor(*bufferHandle);
    }
    bufferHandle.reset();
}

::vk::Buffer& VkBuffer::GetBuffer()
{
    return *buffer;
}

RHIBuffer* VkBuffer::GetStagingBuffer()
{
    return stagingBuffer ? stagingBuffer.get() : nullptr;
}

} // namespace vex::vk

::vk::AccessFlags2 vex::BufferUtil::GetAccessFlagsFromBufferState(RHIBufferState::Flags flags)
{
    using enum ::vk::AccessFlagBits2;
    ::vk::AccessFlags2 accessFlags;
    if (flags & RHIBufferState::ConstantBuffer)
    {
        accessFlags |= eUniformRead;
    }

    if (flags & RHIBufferState::StructuredBuffer)
    {
        accessFlags |= eShaderWrite | eShaderRead;
    }

    if (flags & RHIBufferState::Common)
    {
        accessFlags |= eNone;
    }

    if (flags & RHIBufferState::CopyDest)
    {
        accessFlags |= eTransferWrite;
    }

    if (flags & RHIBufferState::CopySource)
    {
        accessFlags |= eTransferRead;
    }

    if (flags & RHIBufferState::IndexBuffer)
    {
        accessFlags |= eIndexRead;
    }

    if (flags & RHIBufferState::VertexBuffer)
    {
        accessFlags |= eVertexAttributeRead;
    }

    return accessFlags;
}