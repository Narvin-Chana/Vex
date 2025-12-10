#pragma once

#include <optional>

#include <Vex/Bindings.h>
#include <Vex/Containers/Span.h>
#include <Vex/Types.h>
#include <Vex/Utility/EnumFlags.h>

namespace vex
{

struct ASHandle : Handle64<ASHandle>
{
};

static constexpr ASHandle GInvalidASHandle;

enum class ASType : u8
{
    BottomLevel, // BLAS, represents the different geometry.
    TopLevel,    // TLAS, represents instances (with transforms) for a specific geometry.
};

// clang-format off

BEGIN_VEX_ENUM_FLAGS(ASGeometryFlags, u8)
	None						= 0,
	Opaque						= 1 << 0,	// This means AnyHit shaders will not be invoked.
	NoDuplicateAnyHitInvocation = 1 << 1,	// Guarantees single AnyHit invocations.
END_VEX_ENUM_FLAGS();

// Flags for Acceleration Structure building. Currently these are not yet implemented.
BEGIN_VEX_ENUM_FLAGS(ASBuildFlags, u8)
	None			= 0,
	AllowUpdate		= 1 << 0,	// Allows for incremental updates to the AccelerationStructure.
	AllowCompaction = 1 << 1,	// Allows for acceleration structure compaction to save memory.
	PreferFastTrace = 1 << 2,	// Optimizes building for raytracing performance. Incompatible with PreferFastBuild.
	PreferFastBuild = 1 << 3,	// Optimizes building for build-speed. Incompatible with PreferFastTrace.
	MinimizeMemory	= 1 << 4,	// Minimizes memory usage.
END_VEX_ENUM_FLAGS();

// clang-format on

struct BLASGeometryDesc
{
    BufferBinding vertexBufferBinding;
    std::optional<BufferBinding> indexBufferBinding;

    // Optional 3x4 GPU buffer of a transform matrix to apply to vertices before building the BLAS.
    std::optional<BufferBinding> transform;

    ASGeometryFlags::Type flags = ASGeometryFlags::None;
};

struct BLASDesc
{
    std::string name;
    ASBuildFlags::Type buildFlags = ASBuildFlags::PreferFastTrace;
};

struct BLASBuildDesc
{
    ASHandle blas;

    // Geometry to include in this BLAS.
    // Typically you'd have only one geometry per BLAS (one mesh or a mesh and its connected parts, eg: a car with its
    // wheels).
    Span<const BLASGeometryDesc> geometry;

    // TODO: handle BLAS update.
};

struct ASInstanceDesc
{
    // 3x4 row-major transform matrix.
    std::array<float, 3 * 4> transform{
        // clang-format off
        1, 0, 0, 0, 
        0, 1, 0, 0,
        0, 0, 1, 0,
        // clang-format onn
    };

    // Custom InstanceID for user usage in RT shaders.
    u32 instanceID : 24 = 0;
    // Custom InstanceMask for user usage in RT shaders.
    u32 instanceMask : 8 = 0xFF;
    // Shader Binding Table (SBT) offset.
    u32 instanceContributionToHitGroupIndex = 0;

    // Handle to this instance's corresponding BLAS.
    ASHandle blasHandle;
};

struct ASDesc
{
    std::string name;
    ASType type;
    ASBuildFlags::Type buildFlags;

    // For building the BLAS.
    Span<const ASGeometryDesc> geometry;

    // Max number of TLAS instances allowed (needed at construction-time).
    u32 maxInstanceCount = 0;
};

struct ASBuildDesc
{
    ASHandle accelerationStructure;

    // BLAS geometry. If different from the AS's creation geometry, this will cause a full BLAS rebuild which can be
    // very expensive..
    Span<const ASGeometryDesc> geometry;

    // TLAS instance.
    Span<const ASInstanceDesc> instances;

    // TODO: figure out how to do TLAS/BLAS updates...
};

} // namespace vex