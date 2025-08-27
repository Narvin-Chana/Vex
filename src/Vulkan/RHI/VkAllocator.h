#pragma once

#include <Vex/NonNullPtr.h>

#include <RHI/RHIAllocator.h>

#include <Vulkan/VkHeaders.h>

namespace vex
{
enum class ResourceMemoryLocality : u8;
}

namespace vex::vk
{
struct VkGPUContext;

namespace AllocatorUtils
{

::vk::MemoryPropertyFlags GetMemoryPropsFromLocality(ResourceMemoryLocality locality);
u32 GetBestSuitedMemoryTypeIndex(::vk::PhysicalDevice device, u32 typeFilter, ::vk::MemoryPropertyFlags flags);

} // namespace AllocatorUtils

class VkAllocator : public RHIAllocatorBase
{
public:
    VkAllocator(NonNullPtr<VkGPUContext> ctx);
    ~VkAllocator();

    std::span<u8> MapAllocation(const Allocation& alloc);
    void UnmapAllocation(const Allocation& alloc);

    std::pair<::vk::DeviceMemory, Allocation> AllocateResource(ResourceMemoryLocality type,
                                                               const ::vk::MemoryRequirements& memoryRequs);
    void FreeResource(const Allocation& alloc);

    ::vk::DeviceMemory GetMemoryFromAllocation(const Allocation& allocation);

protected:
    virtual void OnPageAllocated(PageHandle handle, u32 memoryTypeIndex) override;
    virtual void OnPageFreed(PageHandle handle, u32 memoryTypeIndex) override;

    // Not using UniqueDeviceMemory here because of wierd quirk with maps in vectors
    std::vector<std::unordered_map<PageHandle, ::vk::DeviceMemory>> memoryPagesByType;

    NonNullPtr<VkGPUContext> ctx;
};

} // namespace vex::vk