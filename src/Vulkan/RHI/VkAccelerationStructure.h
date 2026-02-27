#pragma once

#include <RHI/RHIAccelerationStructure.h>

namespace vex::vk
{
class VkAccelerationStructure final : public RHIAccelerationStructureBase
{
public:
    VkAccelerationStructure(NonNullPtr<VkGPUContext> ctx, const ASDesc& desc);

    virtual const RHIAccelerationStructureBuildInfo& SetupBLASBuild(RHIAllocator& allocator,
                                                                    const RHIBLASBuildDesc& desc) override;
    virtual const RHIAccelerationStructureBuildInfo& SetupTLASBuild(RHIAllocator& allocator,
                                                                    const RHITLASBuildDesc& desc) override;

    virtual std::vector<std::byte> GetInstanceBufferData(const RHITLASBuildDesc& desc) override;
    virtual u32 GetInstanceBufferStride() override;

    std::vector<u32> geometryCount;
    std::vector<::vk::AccelerationStructureGeometryKHR> geometries;
    std::vector<::vk::AccelerationStructureBuildRangeInfoKHR> ranges;
    NonNullPtr<VkGPUContext> ctx;

    ::vk::UniqueAccelerationStructureKHR vkAccelerationStructure;

    MaybeUninitialized<RHIBuffer> instanceBuffer;

    ::vk::DeviceAddress GetNativeAddress();

    void BuildAccelerationStructure(::vk::AccelerationStructureTypeKHR type, RHIAllocator& allocator);
};

::vk::GeometryInstanceFlagsKHR ASInstanceFlagsToVkGeometryInstanceFlags(ASInstance::Flags flags);
::vk::GeometryFlagsKHR GeometryFlagsToVkGeometryFlags(ASGeometry::Flags flags);
::vk::BuildAccelerationStructureFlagsKHR ASBuildFlagsToVkASBuildFlags(ASBuild::Flags flags);
} // namespace vex::vk