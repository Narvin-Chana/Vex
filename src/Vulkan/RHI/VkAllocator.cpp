#include "VkAllocator.h"

#include <utility>

#include <Vex/Resource.h>

#include <Vulkan/VkDebug.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

namespace AllocatorUtils
{

::vk::MemoryPropertyFlags GetMemoryPropsFromLocality(ResourceMemoryLocality locality)
{
    using enum ::vk::MemoryPropertyFlagBits;

    switch (locality)
    {
    case ResourceMemoryLocality::GPUOnly:
        return eDeviceLocal;
    case ResourceMemoryLocality::CPURead:
        return eHostCoherent | eHostVisible | eHostCached;
    case ResourceMemoryLocality::CPUWrite:
        return eHostCoherent | eHostVisible;
    default:
        VEX_LOG(Fatal, "Unable to deduce memory properties from locality");
    }

    std::unreachable();
}

u32 GetBestSuitedMemoryTypeIndex(::vk::PhysicalDevice device, u32 typeFilter, ::vk::MemoryPropertyFlags flags)
{
    ::vk::PhysicalDeviceMemoryProperties memProperties = device.getMemoryProperties();

    for (u32 i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & flags) == flags)
        {
            return i;
        }
    }

    VEX_LOG(Fatal, "Unsuitable memory found for flags {:x}", static_cast<u32>(flags))
    return 0;
}

} // namespace AllocatorUtils

VkAllocator::VkAllocator(NonNullPtr<VkGPUContext> ctx)
    : RHIAllocatorBase(ctx->physDevice.getMemoryProperties().memoryTypeCount)
    , ctx{ ctx }
{
    memoryPagesByType.resize(pageInfos.size());
}

VkAllocator::~VkAllocator()
{
    for (u32 i = 0; i < pageInfos.size(); ++i)
    {
        for (auto& [_, value] : memoryPagesByType[i])
            ctx->device.freeMemory(value);
    }
    memoryPagesByType.clear();
}

std::span<u8> VkAllocator::MapAllocation(const Allocation& alloc)
{
    ::vk::DeviceMemory memory = GetMemoryFromAllocation(alloc);
    void* ptr = VEX_VK_CHECK <<= ctx->device.mapMemory(memory, alloc.memoryRange.offset, alloc.memoryRange.size);
    return { static_cast<u8*>(ptr), alloc.memoryRange.size };
}

void VkAllocator::UnmapAllocation(const Allocation& alloc)
{
    ctx->device.unmapMemory(GetMemoryFromAllocation(alloc));
}

std::pair<::vk::DeviceMemory, Allocation> VkAllocator::AllocateResource(ResourceMemoryLocality memLocality,
                                                                        const ::vk::MemoryRequirements& memoryRequs)
{
    ::vk::MemoryPropertyFlags memPropFlags = AllocatorUtils::GetMemoryPropsFromLocality(memLocality);
    u32 memoryTypeIndex =
        AllocatorUtils::GetBestSuitedMemoryTypeIndex(ctx->physDevice, memoryRequs.memoryTypeBits, memPropFlags);

    Allocation alloc = Allocate(memoryRequs.size, memoryRequs.alignment, memoryTypeIndex);
    ::vk::DeviceMemory memory = memoryPagesByType[memoryTypeIndex].at(alloc.pageHandle);
    return { memory, alloc };
}

void VkAllocator::FreeResource(const Allocation& alloc)
{
    Free(alloc);
}

::vk::DeviceMemory VkAllocator::GetMemoryFromAllocation(const Allocation& allocation)
{
    return memoryPagesByType[allocation.memoryTypeIndex].at(allocation.pageHandle);
}

void VkAllocator::OnPageAllocated(PageHandle handle, u32 memoryTypeIndex)
{
    auto& heapList = memoryPagesByType[memoryTypeIndex];
    u64 pageByteSize = pageInfos[memoryTypeIndex][handle].GetByteSize();

    ::vk::DeviceMemory allocatedMemory = VEX_VK_CHECK <<=
        ctx->device.allocateMemory({ .allocationSize = pageByteSize, .memoryTypeIndex = memoryTypeIndex });

    SetDebugName(
        ctx->device,
        allocatedMemory,
        std::format("Allocated Memory Page (type: {}, handle: {})", memoryTypeIndex, handle.GetIndex()).c_str());

    heapList.insert_or_assign(handle, allocatedMemory);
}

void VkAllocator::OnPageFreed(PageHandle handle, u32 memoryTypeIndex)
{
    ctx->device.freeMemory(memoryPagesByType[memoryTypeIndex].at(handle));
    memoryPagesByType[memoryTypeIndex].erase(handle);
}

} // namespace vex::vk