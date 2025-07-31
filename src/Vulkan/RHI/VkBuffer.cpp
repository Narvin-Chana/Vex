#include "VkBuffer.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex/Buffer.h>

#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>
#include <Vulkan/VkMemory.h>

namespace vex::vk
{
static ::vk::BufferUsageFlags GetVkBufferUsageFromDesc(const BufferDescription& desc)
{
    using enum ::vk::BufferUsageFlagBits;
    ::vk::BufferUsageFlags flags;
    if (desc.usage & BufferUsage::ShaderRead)
    {
        flags |= eStorageBuffer;
    }
    if (desc.usage & BufferUsage::VertexBuffer)
    {
        flags |= eVertexBuffer;
    }
    if (desc.usage & BufferUsage::IndexBuffer)
    {
        flags |= eIndexBuffer;
    }
    if (desc.usage & BufferUsage::IndirectArgs)
    {
        flags |= eIndirectBuffer;
    }
    if (desc.usage & BufferUsage::RaytracingAccelerationStructure)
    {
        flags |= eAccelerationStructureStorageKHR;
    }

    if (!flags)
    {
        VEX_LOG(Fatal, "Buffer usage not supported by VulkanRHI.");
    }

    return flags;
}

static ::vk::MemoryPropertyFlags GetMemoryPropsFromDesc(const BufferDescription& desc)
{
    using enum ::vk::MemoryPropertyFlagBits;

    bool GPURead = (desc.usage & BufferUsage::ShaderRead) > 0;
    GPURead |= (desc.usage & BufferUsage::ShaderReadWrite) > 0;
    GPURead |= (desc.usage & BufferUsage::VertexBuffer) > 0;
    GPURead |= (desc.usage & BufferUsage::IndexBuffer) > 0;
    GPURead |= (desc.usage & BufferUsage::IndirectArgs) > 0;
    GPURead |= (desc.usage & BufferUsage::RaytracingAccelerationStructure) > 0;

    bool GPUWrite = desc.usage & BufferUsage::ShaderReadWrite;
    bool CPURead = desc.usage & BufferUsage::CPUVisible;
    bool CPUWrite = desc.usage & BufferUsage::CPUWrite;

    if (!CPUWrite && !CPURead && (GPUWrite || GPURead))
    {
        return eDeviceLocal;
    }
    else if (GPUWrite || GPURead)
    {
        return eHostCoherent | eHostVisible;
    }
    else
    {
        VEX_LOG(Fatal, "Unable to deduce memory properties from BufferDescription of {}", desc.name);
    }

    std::unreachable();
}

VkBuffer::VkBuffer(VkGPUContext& ctx, const BufferDescription& desc)
    : RHIBufferInterface{ desc }
    , ctx{ ctx }
{
    auto bufferUsage = GetVkBufferUsageFromDesc(desc);
    auto memoryProps = GetMemoryPropsFromDesc(desc);

    if (memoryProps & ::vk::MemoryPropertyFlagBits::eDeviceLocal)
    {
        // Needs to get its data from somewhere. Will therefore always need a transfer dest usage
        bufferUsage |= ::vk::BufferUsageFlagBits::eTransferDst;
    }

    buffer = VEX_VK_CHECK <<=
        ctx.device.createBufferUnique({ .size = desc.byteSize,
                                        .usage = bufferUsage,
                                        .sharingMode = ::vk::SharingMode::eExclusive,
                                        .queueFamilyIndexCount = 1,
                                        .pQueueFamilyIndices = &ctx.graphicsPresentQueue.family });

    const ::vk::MemoryRequirements reqs = ctx.device.getBufferMemoryRequirements(*buffer);

    memory = VEX_VK_CHECK <<= ctx.device.allocateMemoryUnique(
        { .allocationSize = reqs.size,
          .memoryTypeIndex = GetBestMemoryType(ctx.physDevice, reqs.memoryTypeBits, memoryProps) });

    VEX_VK_CHECK << ctx.device.bindBufferMemory(*buffer, *memory, 0);
}

BindlessHandle VkBuffer::GetOrCreateBindlessIndex(VkGPUContext& ctx, VkDescriptorPool& descriptorPool)
{
    if (bufferHandle)
    {
        return *bufferHandle;
    }

    if (desc.stride != 0)
    {
        VEX_LOG(Fatal,
                "VulkanRHI currently does not support StructuredBuffer<> hlsl type due to restrictions with bindless. "
                "This could be fixed in the future using DeviceAddressBuffers, but in the meantime, set stride to 0 "
                "and use a (RW)ByteAddressBuffer in your shaders.");
    }

    const BindlessHandle handle = descriptorPool.AllocateStaticDescriptor(*this);

    descriptorPool.UpdateDescriptor(
        ctx,
        handle,
        ::vk::DescriptorBufferInfo{ .buffer = *buffer, .offset = 0, .range = desc.byteSize });

    bufferHandle = handle;

    return handle;
}

void VkBuffer::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    if (bufferHandle && *bufferHandle != GInvalidBindlessHandle)
    {
        descriptorPool.FreeStaticDescriptor(*bufferHandle);
    }
    bufferHandle.reset();
}

::vk::Buffer VkBuffer::GetNativeBuffer()
{
    return *buffer;
}

UniqueHandle<VkBuffer> VkBuffer::CreateStagingBuffer()
{
    return MakeUnique<VkBuffer>(ctx,
                                BufferDescription{
                                    .name = desc.name + "_StagingBuffer",
                                    .byteSize = desc.byteSize,
                                    .usage = BufferUsage::CPUWrite,
                                });
}

std::span<u8> VkBuffer::Map()
{
    void* ptr = VEX_VK_CHECK <<= ctx.device.mapMemory(*memory, 0, desc.byteSize);
    return { static_cast<u8*>(ptr), desc.byteSize };
}

void VkBuffer::Unmap()
{
    ctx.device.unmapMemory(*memory);
}

} // namespace vex::vk

::vk::AccessFlags2 vex::BufferUtil::GetAccessFlagsFromBufferState(RHIBufferState::Flags flags)
{
    using enum ::vk::AccessFlagBits2;
    ::vk::AccessFlags2 accessFlags = eNone;

    if (flags & RHIBufferState::CopySource)
    {
        accessFlags |= eTransferRead;
    }

    if (flags & RHIBufferState::CopyDest)
    {
        accessFlags |= eTransferWrite;
    }

    if (flags & RHIBufferState::UniformResource)
    {
        accessFlags |= eUniformRead;
    }

    if (flags & RHIBufferState::ShaderResource)
    {
        accessFlags |= eShaderRead;
    }

    if (flags & RHIBufferState::ShaderReadWrite)
    {
        accessFlags |= eShaderWrite;
    }

    if (flags & RHIBufferState::VertexBuffer)
    {
        accessFlags |= eVertexAttributeRead;
    }

    if (flags & RHIBufferState::IndexBuffer)
    {
        accessFlags |= eIndexRead;
    }

    if (flags & RHIBufferState::IndirectArgs)
    {
        accessFlags |= eIndirectCommandRead;
    }

    if (flags & RHIBufferState::RaytracingAccelerationStructure)
    {
        accessFlags |= eAccelerationStructureReadKHR;
    }

    return accessFlags;
}