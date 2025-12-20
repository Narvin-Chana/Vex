#pragma once

#include <optional>
#include <string>

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

struct AccelerationStructure
{
    ASHandle handle;
    ASType type;
};

// clang-format off

BEGIN_VEX_ENUM_FLAGS(ASGeometryFlags, u8)
	None						= 0,
	Opaque						= 1 << 0,	// This means AnyHit shaders will not be invoked.
	NoDuplicateAnyHitInvocation = 1 << 1,	// Guarantees single AnyHit invocations.
END_VEX_ENUM_FLAGS();

BEGIN_VEX_ENUM_FLAGS(ASInstanceFlags, u8)
	None						  = 0,
	TriangleCullDisable			  = 1 << 0,
	TriangleFrontCounterClockwise = 1 << 1,
	ForceOpaque                   = 1 << 2,
	ForceNonOpaque                = 1 << 3,
	// TODO: Opacity Micro-Maps flags.
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
    // Geometry vertices.
    BufferBinding vertexBufferBinding;
    // Optional index buffer for the geometry.
    std::optional<BufferBinding> indexBufferBinding;
    // Optional 3x4 transform matrix to apply to vertices before building the BLAS.
    std::optional<std::array<float, 3 * 4>> transform;

    ASGeometryFlags::Type flags = ASGeometryFlags::Opaque;
};

struct BLASDesc
{
    std::string name;
    ASBuildFlags::Type buildFlags = ASBuildFlags::PreferFastTrace;
};

struct BLASBuildDesc
{
    // Geometry to include in this BLAS.
    // Typically you'd have only one geometry per BLAS (one mesh or a mesh and its connected parts, eg: a car with its
    // wheels).
    Span<const BLASGeometryDesc> geometry;

    // TODO: handle BLAS update.
};

struct TLASInstanceDesc
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
    // Flags for the instance.
    ASInstanceFlags::Flags instanceFlags = ASInstanceFlags::None;

    // Handle to this instance's corresponding BLAS.
    AccelerationStructure blas;
};

static constexpr u32 GTLASDeduceInstanceCountFromFirstBuild = ~0u;
struct TLASDesc
{
    std::string name;
    // Number of instances to allow for in this TLAS.
    // If set to the default value, Vex will set the max instance count to the number of instances passed in during the first build.
    u32 maxInstanceCount = GTLASDeduceInstanceCountFromFirstBuild;
};

struct TLASBuildDesc
{
    // TLAS instances.
    Span<const TLASInstanceDesc> instances;
};

} // namespace vex