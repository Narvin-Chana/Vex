#include "VkBuffer.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex/Bindings.h>
#include <Vex/Buffer.h>

#include <Vulkan/RHI/VkAllocator.h>
#include <Vulkan/VkCommandQueue.h>
#include <Vulkan/VkDebug.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{
static ::vk::BufferUsageFlags GetVkBufferUsageFromDesc(const BufferDesc& desc)
{
    using enum ::vk::BufferUsageFlagBits;
    // TODO: Figure out implication of eAccelerationStructureBuildInputReadOnlyKHR needed when uploading to AS
    ::vk::BufferUsageFlags flags = eTransferSrc | eShaderDeviceAddress | eAccelerationStructureBuildInputReadOnlyKHR;
    if (desc.usage & BufferUsage::UniformBuffer)
    {
        flags |= eUniformBuffer;
    }
    if (desc.usage & (BufferUsage::ReadWriteBuffer | BufferUsage::GenericBuffer))
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
    if (desc.usage & BufferUsage::AccelerationStructure)
    {
        flags |= eAccelerationStructureStorageKHR;
    }
    return flags;
}

VkBuffer::VkBuffer(NonNullPtr<VkGPUContext> ctx, VkAllocator& allocator, const BufferDesc& desc)
    : RHIBufferBase{ allocator, desc }
    , ctx{ ctx }
{
    auto bufferUsage = GetVkBufferUsageFromDesc(desc);

    if (desc.memoryLocality == ResourceMemoryLocality::GPUOnly ||
        desc.memoryLocality == ResourceMemoryLocality::CPURead)
    {
        // Needs to get its data from somewhere. Will therefore always need a transfer dest usage
        bufferUsage |= ::vk::BufferUsageFlagBits::eTransferDst;
    }

    buffer = VEX_VK_CHECK <<=
        ctx->device.createBufferUnique({ .size = desc.byteSize,
                                         .usage = bufferUsage,
                                         .sharingMode = ::vk::SharingMode::eExclusive,
                                         .queueFamilyIndexCount = 1,
                                         .pQueueFamilyIndices = &ctx->graphicsPresentQueue.family });

    const ::vk::MemoryRequirements reqs = ctx->device.getBufferMemoryRequirements(*buffer);

#if VEX_USE_CUSTOM_ALLOCATOR_BUFFERS
    auto [memory, newAllocation] = allocator.AllocateResource(desc.memoryLocality, reqs);
    allocation = newAllocation;
    VEX_VK_CHECK << ctx->device.bindBufferMemory(*buffer, memory, allocation.memoryRange.offset);
#else
    // TODO(https://trello.com/c/4CKvUpd2): Fix the non-custom allocator buffer codepath for VkBuffers.
    ::vk::MemoryPropertyFlags memPropFlags = AllocatorUtils::GetMemoryPropsFromLocality(desc.memoryLocality);
    memory = VEX_VK_CHECK <<= ctx->device.allocateMemoryUnique(
        { .allocationSize = reqs.size,
          .memoryTypeIndex =
              AllocatorUtils::GetBestSuitedMemoryTypeIndex(ctx->physDevice, reqs.memoryTypeBits, memPropFlags) });

    SetDebugName(ctx->device, *memory, std::format("Memory: {}", desc.name).c_str());
    VEX_VK_CHECK << ctx->device.bindBufferMemory(*buffer, *memory, 0);
#endif

    if (IsMappable())
    {
#if VEX_USE_CUSTOM_ALLOCATOR_BUFFERS
        mappedData = allocator.GetMappedDataFromAllocation(allocation);
#else
        void* ptr = VEX_VK_CHECK <<= ctx->device.mapMemory(*memory, 0, VK_WHOLE_SIZE);
        mappedData = { static_cast<byte*>(ptr), desc.byteSize };
#endif
    }

    SetDebugName(ctx->device, *buffer, std::format("Buffer: {}", desc.name).c_str());
}

void VkBuffer::AllocateBindlessHandle(RHIDescriptorPool& descriptorPool,
                                      BindlessHandle handle,
                                      const BufferViewDesc& viewDesc)
{
    descriptorPool.GetBindlessSet().UpdateDescriptor(handle,
                                                     desc.usage == BufferUsage::UniformBuffer
                                                         ? ::vk::DescriptorType::eUniformBuffer
                                                         : ::vk::DescriptorType::eStorageBuffer,
                                                     ::vk::DescriptorBufferInfo{ .buffer = *buffer,
                                                                                 .offset = viewDesc.offsetByteSize,
                                                                                 .range = viewDesc.rangeByteSize });
}

::vk::Buffer VkBuffer::GetNativeBuffer()
{
    return *buffer;
}

::vk::DeviceAddress VkBuffer::GetDeviceAddress()
{
    ::vk::BufferDeviceAddressInfo info{ .buffer = *buffer };
    return VEX_VK_CHECK <<= ctx->device.getBufferAddress(info);
}

} // namespace vex::vk
