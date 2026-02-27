#pragma once

#include <string>

#include <Vex/Types.h>
#include <Vex/Utility/EnumFlags.h>
#include <Vex/Utility/Handle.h>
#include <Vex/Utility/Hash.h>

namespace vex
{

struct ASHandle : Handle64<ASHandle>
{
};

static constexpr ASHandle GInvalidASHandle;

enum class ASType : u8
{
    BottomLevel, // BLAS, represents the different geometry.
    TopLevel,    // TLAS, represents instances (with transforms) for specific BLAS.
};

// clang-format off

// Flags for Acceleration Structure building. Currently most of these are not yet implemented.
BEGIN_VEX_ENUM_FLAGS(ASBuild, u8)
	None			= 0,
	AllowUpdate		= 1 << 0,	// Allows for incremental updates to the AccelerationStructure.
	AllowCompaction = 1 << 1,	// Allows for acceleration structure compaction to save memory.
	PreferFastTrace = 1 << 2,	// Optimizes building for raytracing performance. Incompatible with PreferFastBuild.
	PreferFastBuild = 1 << 3,	// Optimizes building for build-speed. Incompatible with PreferFastTrace.
	MinimizeMemory	= 1 << 4,	// Minimizes memory usage.
    // TODO(https://trello.com/c/LIEtASpP): Updating AS is not currently supported.
    PerformUpdate   = 1 << 5,   // Allows for updating the AS.
END_VEX_ENUM_FLAGS();

// clang-format on

struct ASDesc
{
    std::string name;
    ASType type;
    ASBuild::Flags buildFlags = ASBuild::PreferFastTrace;

    constexpr bool operator==(const ASDesc&) const = default;
};

struct AccelerationStructure
{
    ASHandle handle;
    ASDesc desc;

    constexpr bool operator==(const AccelerationStructure&) const = default;
};

} // namespace vex

// clang-format off

VEX_MAKE_HASHABLE(vex::ASDesc,
    VEX_HASH_COMBINE(seed, obj.name);
    VEX_HASH_COMBINE(seed, obj.type);
    VEX_HASH_COMBINE(seed, obj.buildFlags);
);

VEX_MAKE_HASHABLE(vex::AccelerationStructure,
    VEX_HASH_COMBINE(seed, obj.handle);
    VEX_HASH_COMBINE(seed, obj.desc);
);

// clang-format on
