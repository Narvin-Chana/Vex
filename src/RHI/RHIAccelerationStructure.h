#pragma once

#include <optional>
#include <variant>

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
    RHIBufferBinding vertexBufferBinding;
    std::optional<RHIBufferBinding> indexBufferBinding;
    std::optional<RHIBufferBinding> transform;

    ASGeometryFlags::Flags flags = ASGeometryFlags::None;
};

// RHI version of BLASBuildDesc
struct RHIBLASBuildDesc
{
    Span<const RHIBLASGeometryDesc> geometry;
};

// RHI version of TLASBuildDesc
struct RHITLASBuildDesc
{
    // Description of each individual instance in the TLAS.
    Span<const TLASInstanceDesc> instanceDescs;
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

    // Potentially required upload buffer for acceleration structure initialization.
    std::optional<u64> uploadBufferByteSize;
};

class RHIAccelerationStructureBase
{
public:
    // BLAS Constructor
    RHIAccelerationStructureBase(const BLASDesc& desc)
        : type(ASType::BottomLevel)
        , desc(desc)
    {
    }

    // TLAS Constructor
    RHIAccelerationStructureBase(const TLASDesc& desc)
        : type(ASType::TopLevel)
        , desc(desc)
    {
    }

    const BLASDesc& GetBLASDesc() const
    {
        VEX_ASSERT(std::holds_alternative<BLASDesc>(desc), "Cannot obtain BLAS desc from a TLAS.");
        return std::get<BLASDesc>(desc);
    }

    const TLASDesc& GetTLASDesc() const
    {
        VEX_ASSERT(std::holds_alternative<TLASDesc>(desc), "Cannot obtain TLAS desc from a BLAS.");
        return std::get<TLASDesc>(desc);
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

    void FreeBindlessHandles(RHIDescriptorPool& descriptorPool);
    void FreeAllocation(RHIAllocator& allocator);

protected:
    ASType type;
    std::variant<BLASDesc, TLASDesc> desc;
    MaybeUninitialized<RHIBuffer> accelerationStructure;
    RHIAccelerationStructureBuildInfo prebuildInfo;
};

} // namespace vex