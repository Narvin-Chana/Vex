#pragma once

#include <optional>
#include <variant>

#include <Vex/AccelerationStructure.h>
#include <Vex/Utility/MaybeUninitialized.h>

#include <RHI/RHIBindings.h>
#include <RHI/RHIFwd.h>

namespace vex
{

// RHI version of BLASGeometryDesc
struct RHIBLASGeometryDesc
{
    RHIBufferBinding vertexBufferBinding;
    std::optional<RHIBufferBinding> indexBufferBinding;
    std::optional<RHIBufferBinding> transform;

    ASGeometryFlags::Type flags = ASGeometryFlags::None;
};

// RHI version of BLASBuildDesc
struct RHIBLASBuildDesc
{
    Span<const RHIBLASGeometryDesc> geometry;
};

// RHI version of TLASBuildDesc
struct RHITLASBuildDesc
{
    // Buffer containing the transforms of each instance.
    RHIBufferBinding perInstanceTransformBufferBinding;
    // Description of each individual instance in the TLAS.
    Span<const TLASInstanceDesc> instanceDescs;
    // Per-instance BLAS to map each TLAS instance to.
    Span<const NonNullPtr<RHIAccelerationStructure>> perInstanceBLAS;
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

protected:
    ASType type;
    std::variant<BLASDesc, TLASDesc> desc;
    MaybeUninitialized<RHIBuffer> accelerationStructure;
};

} // namespace vex