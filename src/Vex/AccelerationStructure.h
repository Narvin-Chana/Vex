#pragma once

#include <string>

#include <Vex/Types.h>
#include <Vex/Utility/EnumFlags.h>
#include <Vex/Utility/Handle.h>
#include <Vex/Utility/Hash.h>

namespace vex
{

struct AccelerationStructureHandle : Handle64<AccelerationStructureHandle>
{
};

static constexpr AccelerationStructureHandle GInvalidASHandle;

enum class ASType : u8
{
    BottomLevel, // BLAS, represents the different geometry.
    TopLevel,    // TLAS, represents instances (with transforms) for specific BLAS.
};

// clang-format off

// Flags for Acceleration Structure building. Currently most of these are not yet implemented.
enum class ASBuild : u8
{
    None			= 0,
    AllowUpdate		= 1 << 0,	// Allows for incremental updates to the AccelerationStructure.
    AllowCompaction = 1 << 1,	// Allows for acceleration structure compaction to save memory.
    PreferFastTrace = 1 << 2,	// Optimizes building for raytracing performance. Incompatible with PreferFastBuild.
    PreferFastBuild = 1 << 3,	// Optimizes building for build-speed. Incompatible with PreferFastTrace.
    MinimizeMemory	= 1 << 4,	// Minimizes memory usage.
    // TODO(https://trello.com/c/LIEtASpP): Updating AS is not currently supported.
    PerformUpdate   = 1 << 5,   // Allows for updating the AS.
};
VEX_ENUM_FLAG_BITS(ASBuild);

// clang-format on

struct AccelerationStructureDesc
{
    std::string name;
    ASType type;
    Flags<ASBuild> buildFlags = ASBuild::PreferFastTrace;

    constexpr bool operator==(const AccelerationStructureDesc&) const = default;
};

struct AccelerationStructure
{
    AccelerationStructureHandle handle;
    AccelerationStructureDesc desc;

    constexpr bool operator==(const AccelerationStructure&) const = default;
};

} // namespace vex

// clang-format off

VEX_MAKE_HASHABLE(vex::AccelerationStructureDesc,
    VEX_HASH_COMBINE(seed, obj.name);
    VEX_HASH_COMBINE(seed, obj.type);
    VEX_HASH_COMBINE(seed, obj.buildFlags);
);

VEX_MAKE_HASHABLE(vex::AccelerationStructure,
    VEX_HASH_COMBINE(seed, obj.handle);
    VEX_HASH_COMBINE(seed, obj.desc);
);

// clang-format on
