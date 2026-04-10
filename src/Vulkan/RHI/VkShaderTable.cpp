#include "VkShaderTable.h"

#include <Vulkan/VkGPUContext.h>

namespace vex::vk
{

VkShaderTable::VkShaderTable(NonNullPtr<VkGPUContext> ctx,
                             RHIAllocator& allocator,
                             std::string_view name,
                             Span<void*> shaderHandles)
{
    auto asProperties =
        ctx->physDevice
            .getProperties2<::vk::PhysicalDeviceProperties2, ::vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>()
            .get<::vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    entrySize = asProperties.shaderGroupBaseAlignment;
    auto handleSize = asProperties.shaderGroupHandleSize;

    auto bufferDesc = BufferDesc::CreateStagingBufferDesc(std::string(name),
                                                          entrySize * shaderHandles.size(),
                                                          BufferUsage::ShaderRead | BufferUsage::ShaderTable);

    shaderTableBuffer = RHIBuffer(ctx, allocator, bufferDesc);

    auto mappedData = shaderTableBuffer->GetMappedData();

    std::byte* dstPtr = mappedData.data();
    std::uninitialized_fill_n(dstPtr, bufferDesc.byteSize, static_cast<std::byte>(0));

    for (u32 i = 0; i < shaderHandles.size(); ++i)
    {
        auto* srcPtr = static_cast<std::byte*>(shaderHandles[i]);
        std::uninitialized_copy_n(srcPtr, handleSize, dstPtr);
        dstPtr += entrySize;
    }
}

::vk::StridedDeviceAddressRegionKHR VkShaderTable::GetDeviceRegion(u32 tableIndex) const
{
    return {
        .deviceAddress = shaderTableBuffer->GetDeviceAddress() + tableIndex * entrySize,
        .stride = entrySize,
        .size = entrySize,
    };
}

} // namespace vex::vk
