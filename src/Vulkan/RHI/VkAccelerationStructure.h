#pragma once

#include <RHI/RHIAccelerationStructure.h>

namespace vex::vk
{
    class VkAccelerationStructure final : public RHIAccelerationStructureBase
    {
    public:
        VkAccelerationStructure(const ASDesc& desc);

        virtual const RHIAccelerationStructureBuildInfo& SetupBLASBuild(RHIAllocator& allocator,
                                                                        const RHIBLASBuildDesc& desc) override;
        virtual const RHIAccelerationStructureBuildInfo& SetupTLASBuild(RHIAllocator& allocator,
                                                                        const RHITLASBuildDesc& desc) override;
    };
}