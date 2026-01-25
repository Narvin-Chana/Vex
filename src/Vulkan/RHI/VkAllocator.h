#pragma once

#include <Vex/Containers/Span.h>
#include <Vex/Utility/NonNullPtr.h>

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
bool IsMemoryTypeIndexMappable(::vk::PhysicalDevice device, u32 memoryTypeIndex);

} // namespace AllocatorUtils

class VkAllocator : public RHIAllocatorBase
{
public:
    VkAllocator(NonNullPtr<VkGPUContext> ctx);
    ~VkAllocator();

    std::pair<::vk::DeviceMemory, Allocation> AllocateResource(ResourceMemoryLocality type,
                                                               const ::vk::MemoryRequirements& memoryRequs);
    void FreeResource(const Allocation& alloc);

    ::vk::DeviceMemory GetMemoryFromAllocation(const Allocation& allocation);
    Span<byte> GetMappedDataFromAllocation(const Allocation& allocation);

protected:
    virtual void OnPageAllocated(PageHandle handle, u32 memoryTypeIndex) override;
    virtual void OnPageFreed(PageHandle handle, u32 memoryTypeIndex) override;

    // Not using UniqueDeviceMemory here because of weird quirk with maps in vectors.
    // Contains a pair of the memory with its (potentially empty) mapped memory.
    std::vector<std::unordered_map<PageHandle, std::pair<::vk::DeviceMemory, Span<byte>>>> memoryPagesByType;

    NonNullPtr<VkGPUContext> ctx;
};

} // namespace vex::vk