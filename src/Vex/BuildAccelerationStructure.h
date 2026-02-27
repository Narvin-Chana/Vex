#pragma once

#include <array>
#include <optional>
#include <vector>

#include <Vex/AccelerationStructure.h>
#include <Vex/Bindings.h>
#include <Vex/Containers/Span.h>
#include <Vex/Types.h>
#include <Vex/Utility/EnumFlags.h>

namespace vex
{

struct AABB
{
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

enum class ASGeometryType : u8
{
    // Geometry is defined as vertex buffers/index buffers.
    Triangles,
    // Geometry is defined as an Axis-Aligned Bounding Box.
    // !! Requires an Intersection shader in your ray tracing pipeline !!
    // The SBT hit group for AABB instances must include an Intersection shader, otherwise ray tracing will fail at
    // dispatch time.
    AABBs,
};

// clang-format off

BEGIN_VEX_ENUM_FLAGS(ASGeometry, u8)
	None						= 0,
	Opaque						= 1 << 0,	// This means AnyHit shaders will not be invoked.
	NoDuplicateAnyHitInvocation = 1 << 1,	// Guarantees single AnyHit invocations.
END_VEX_ENUM_FLAGS();

BEGIN_VEX_ENUM_FLAGS(ASInstance, u8)
	None						  = 0,
	TriangleCullDisable			  = 1 << 0,
	TriangleFrontCounterClockwise = 1 << 1,
	ForceOpaque                   = 1 << 2,
	ForceNonOpaque                = 1 << 3,
	// TODO(https://trello.com/c/YPn5ypzR): Opacity Micro-Maps flags.
END_VEX_ENUM_FLAGS();

// clang-format on

struct BLASGeometryDesc
{
    // For Triangles:
    // Geometry vertices.
    BufferBinding vertexBufferBinding;
    // Optional index buffer for the geometry.
    std::optional<BufferBinding> indexBufferBinding;
    // Optional 3x4 transform matrix to apply to vertices before building the BLAS.
    std::optional<std::array<float, 3 * 4>> transform;

    // For AABBs:
    // Buffer containing D3D12_RAYTRACING_AABB or VkAabbPositionsKHR
    std::vector<AABB> aabbs;

    ASGeometry::Flags flags = ASGeometry::Opaque;
};

struct BLASBuildDesc
{
    ASGeometryType type = ASGeometryType::Triangles;

    // Geometry to include in this BLAS.
    // Typically you'd have only one geometry per BLAS (one mesh or a mesh and its connected parts, eg: a car with its
    // wheels).
    Span<const BLASGeometryDesc> geometry;

    // TODO(https://trello.com/c/LIEtASpP): handle BLAS update.
};

struct TLASInstanceDesc
{
    // 3x4 row-major transform matrix.
    std::array<float, 3 * 4> transform{
        // clang-format off
        1, 0, 0, 0, 
        0, 1, 0, 0,
        0, 0, 1, 0,
        // clang-format on
    };

    // Custom InstanceID for user usage in RT shaders.
    u32 instanceID : 24 = 0;
    // Custom InstanceMask for user usage in RT shaders.
    u32 instanceMask : 8 = 0xFF;
    // Shader Binding Table (SBT) offset.
    u32 instanceContributionToHitGroupIndex = 0;
    // Flags for the instance.
    ASInstance::Flags instanceFlags = ASInstance::ForceOpaque;

    // Handle to this instance's corresponding BLAS.
    AccelerationStructure blas;
};

struct TLASBuildDesc
{
    // TLAS instances.
    Span<const TLASInstanceDesc> instances;
};

} // namespace vex