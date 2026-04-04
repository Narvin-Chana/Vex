#pragma once
#include <Vex/Containers/Span.h>

#include <RHI/RHIFwd.h>

#include <Vulkan/RHI/VkBuffer.h>

namespace vex::vk
{
struct VkGPUContext;

class VkShaderTable
{
public:
    VkShaderTable(NonNullPtr<VkGPUContext> ctx,
                  RHIAllocator& allocator,
                  std::string_view name,
                  Span<void*> shaderHandles);

    ::vk::StridedDeviceAddressRegionKHR GetDeviceRegion(u32 tableIndex) const;

private:
    MaybeUninitialized<RHIBuffer> shaderTableBuffer;
    u32 entrySize;

    friend class VkRayTracingPipelineState;
};

} // namespace vex::vk
