#pragma once

#include <optional>

#include <Vex/AccelerationStructure.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/Utility/MaybeUninitialized.h>

#include <RHI/RHIBindings.h>
#include <RHI/RHIBuffer.h>
#include <RHI/RHIFwd.h>

namespace vex
{

// RHI version of BLASGeometryDesc
struct RHIBLASGeometryDesc
{
    // For Triangles:
    std::optional<RHIBufferBinding> vertexBufferBinding;
    std::optional<RHIBufferBinding> indexBufferBinding;
    std::optional<RHIBufferBinding> transformBufferBinding;

    // For AABBs:
    // Buffer containing D3D12_RAYTRACING_AABB or VkAabbPositionsKHR
    std::optional<RHIBufferBinding> aabbBufferBinding;

    ASGeometry::Flags flags = ASGeometry::None;
};

// RHI version of BLASBuildDesc
struct RHIBLASBuildDesc
{
    ASGeometryType type = ASGeometryType::Triangles;
    Span<const RHIBLASGeometryDesc> geometries;
};

// RHI version of TLASBuildDesc
struct RHITLASBuildDesc
{
    std::optional<RHIBufferBinding> instancesBinding;
    // Description of each individual instance in the TLAS.
    Span<const TLASInstanceDesc> instances;
    // Per-instance BLAS to map each TLAS instance to.
    Span<const NonNullPtr<RHIAccelerationStructure>> perInstanceBLAS;
};

struct RHIAccelerationStructureBuildInfo
{
    // Required size to store the acceleration structure.
    u64 asByteSize;
    // Required size to build the acceleration structure.
    u64 scratchByteSize;
    // Required size to update the acceleration structure.
    u64 updateScratchByteSize;
};

class RHIAccelerationStructureBase
{
public:
    RHIAccelerationStructureBase(const ASDesc& desc)
        : desc(desc)
    {
    }

    const ASDesc& GetDesc() const
    {
        return desc;
    }

    const RHIBuffer& GetRHIBuffer() const
    {
        return *accelerationStructure;
    }

    RHIBuffer& GetRHIBuffer()
    {
        return *accelerationStructure;
    }

    virtual const RHIAccelerationStructureBuildInfo& SetupBLASBuild(RHIAllocator& allocator,
                                                                    const RHIBLASBuildDesc& desc) = 0;
    virtual const RHIAccelerationStructureBuildInfo& SetupTLASBuild(RHIAllocator& allocator,
                                                                    const RHITLASBuildDesc& desc) = 0;

    virtual std::vector<std::byte> GetInstanceBufferData(const RHITLASBuildDesc& desc) = 0;
    virtual u32 GetInstanceBufferStride() = 0;

    void FreeBindlessHandles(RHIDescriptorPool& descriptorPool);
    void FreeAllocation(RHIAllocator& allocator);

protected:
    ASDesc desc;
    MaybeUninitialized<RHIBuffer> accelerationStructure;
    RHIAccelerationStructureBuildInfo prebuildInfo;
};

} // namespace vex