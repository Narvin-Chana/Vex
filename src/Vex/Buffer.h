#pragma once

#include <string>

#include <Vex/EnumFlags.h>
#include <Vex/Handle.h>
#include <Vex/Logger.h>
#include <Vex/Resource.h>
#include <Vex/Types.h>

namespace vex
{

// clang-format off

// Determines how the buffer can be used.
BEGIN_VEX_ENUM_FLAGS(BufferUsage, u8)
    None                            = 0,      // Buffers that will never be bound anywhere. Mostly used for staging buffers.
    GenericBuffer                   = 1 << 0, // Buffers that can be read from shaders (SRV)
    UniformBuffer                   = 1 << 1, // Buffers with specific alignment constraints (CBV)
    ReadWriteBuffer                 = 1 << 2, // Buffers with read and write operations in shaders (UAV)
    VertexBuffer                    = 1 << 3, // Buffers used for vertex buffers
    IndexBuffer                     = 1 << 4, // Buffers used for index buffers
    IndirectArgs                    = 1 << 5, // Buffers used as parameters for an indirect dispatch.
    RaytracingAccelerationStructure = 1 << 6, // Buffers used as an RT Acceleration Structure.
END_VEX_ENUM_FLAGS();

// clang-format on

// Defines what the specific binding will bind as
// maps directly to the type that will be used in the
// shader to access the buffer
enum class BufferBindingUsage : u8
{
    ConstantBuffer,
    StructuredBuffer,
    RWStructuredBuffer,
    ByteAddressBuffer,
    RWByteAddressBuffer,
    Invalid = 0xFF
};

inline bool IsBindingUsageCompatibleWithBufferUsage(BufferUsage::Flags usages, BufferBindingUsage bindingUsage)
{
    if (bindingUsage == BufferBindingUsage::ConstantBuffer)
    {
        return usages & BufferUsage::UniformBuffer;
    }

    if (bindingUsage == BufferBindingUsage::StructuredBuffer || bindingUsage == BufferBindingUsage::ByteAddressBuffer)
    {
        return usages & BufferUsage::GenericBuffer;
    }

    if (bindingUsage == BufferBindingUsage::RWStructuredBuffer ||
        bindingUsage == BufferBindingUsage::RWByteAddressBuffer)
    {
        return usages & BufferUsage::ReadWriteBuffer;
    }

    return true;
}

struct BufferDesc
{
    std::string name;
    u64 byteSize = 0;
    BufferUsage::Flags usage = BufferUsage::GenericBuffer;
    ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly;
};

// Strongly defined type represents a buffer.
// We use a struct (instead of a typedef/using) to enforce compile-time correctness of handles.
struct BufferHandle : public Handle<BufferHandle>
{
};

static constexpr BufferHandle GInvalidBufferHandle;

struct Buffer final
{
    BufferHandle handle;
    BufferDesc desc;
};

static constexpr u64 GBufferWholeSize = ~static_cast<u64>(0);

struct BufferRegion
{
    u64 offset = 0;
    u64 size = GBufferWholeSize;

    // Inserts actual size instead of the placeholder value.
    BufferRegion Resolve(const BufferDesc& desc) const;
    constexpr bool operator==(const BufferRegion&) const = default;
};

struct BufferCopyDesc
{
    u64 srcOffset;
    u64 dstOffset;
    u64 size = GBufferWholeSize;

    // Inserts the actual size instead of the placeholder value.
    BufferCopyDesc Resolve(const BufferDesc& srcBuffer, const BufferDesc& dstBuffer) const;
    constexpr bool operator==(const BufferCopyDesc&) const = default;
};

namespace BufferUtil
{

void ValidateBufferDesc(const BufferDesc& desc);
void ValidateBufferCopyDesc(const BufferDesc& srcDesc, const BufferDesc& dstDesc, const BufferCopyDesc& copyDesc);
void ValidateBufferRegion(const BufferDesc& desc, const BufferRegion& region);
void ValidateSimpleBufferCopy(const BufferDesc& srcDesc, const BufferDesc& dstDesc);

} // namespace BufferUtil

} // namespace vex
