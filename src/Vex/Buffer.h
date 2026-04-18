#pragma once

#include <string>

#include <Vex/Resource.h>
#include <Vex/Types.h>
#include <Vex/Utility/EnumFlags.h>
#include <Vex/Utility/Handle.h>

namespace vex
{

// clang-format off

// Determines how the buffer can be used.
BEGIN_VEX_ENUM_FLAGS(BufferUsage, u16)
    // Buffers that will never be bound anywhere. Mostly used for staging buffers.
    None                        = 0,
    // Buffers that can be read from shaders via Structured or ByteAddress reads.
    ShaderRead                  = 1 << 0,
    // Buffers with specific alignment constraints and uniformly read across waves.
    ShaderReadUniform           = 1 << 1,
    // Buffers with read and write operations in shaders via RWStructured or RWByteAddress read/writes.
    ShaderReadWrite             = 1 << 2,
    // Buffers used for vertex buffers.
    VertexBuffer                = 1 << 3,
    // Buffers used for index buffers.
    IndexBuffer                 = 1 << 4,
    // Buffers used as parameters for an indirect dispatch.
    IndirectArgs                = 1 << 5,
    // TODO(https://trello.com/c/92AYN8Oz): check/rework this, error prone
    // Buffers used as a HWRT Acceleration Structure, these also require the ShaderReadWrite usage.
    AccelerationStructure       = (1 << 6) | ShaderReadWrite,
    // Buffers used as a scratch buffer for building HWRT Acceleration Structures, these also require the ShaderReadWrite usage.
    Scratch                     = (1 << 7) | ShaderReadWrite,
    // Buffers used as inputs to acceleration structure builds (i.e. vertex, index buffers)
    BuildAccelerationStructure  = 1 << 8,
    // Buffers used as a ShaderTable for HWRT shaders.
    ShaderTable                 = 1 << 9,
END_VEX_ENUM_FLAGS();

// clang-format on

// Defines what the specific binding will bind as maps directly to the type that will be used in the shader to access
// the buffer.
enum class BufferBindingUsage : u8
{
    UniformBuffer,
    StructuredBuffer,
    RWStructuredBuffer,
    ByteAddressBuffer,
    RWByteAddressBuffer,
    Invalid = 0xFF
};

inline bool IsBindingUsageCompatibleWithBufferUsage(BufferUsage::Flags usages, BufferBindingUsage bindingUsage)
{
    if (bindingUsage == BufferBindingUsage::UniformBuffer)
    {
        return usages & BufferUsage::ShaderReadUniform;
    }

    if (bindingUsage == BufferBindingUsage::StructuredBuffer || bindingUsage == BufferBindingUsage::ByteAddressBuffer)
    {
        return usages & BufferUsage::ShaderRead;
    }

    if (bindingUsage == BufferBindingUsage::RWStructuredBuffer ||
        bindingUsage == BufferBindingUsage::RWByteAddressBuffer)
    {
        return usages & BufferUsage::ShaderReadWrite;
    }

    return true;
}

struct BufferDesc
{
    std::string name;
    u64 byteSize = 0;
    BufferUsage::Flags usage = BufferUsage::ShaderRead;
    ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly;

    // Helpers to create a buffer description.

    // Creates a CPUWrite buffer useable as a Uniform (/constant) Buffer.
    static BufferDesc CreateUniformBufferDesc(std::string name, u64 byteSize);
    // Creates a GPUOnly buffer useable as an Vertex Buffer.
    static BufferDesc CreateVertexBufferDesc(std::string name,
                                             u64 byteSize,
                                             bool allowShaderRead = false,
                                             bool canBeAccelerationStructureSource = false);
    // Creates a GPUOnly buffer useable as an Index Buffer.
    static BufferDesc CreateIndexBufferDesc(std::string name,
                                            u64 byteSize,
                                            bool allowShaderRead = false,
                                            bool canBeAccelerationStructureSource = false);
    // Creates a CPUWrite staging buffer useful for uploading data to GPUOnly resources.
    static BufferDesc CreateStagingBufferDesc(std::string name,
                                              u64 byteSize,
                                              BufferUsage::Flags usageFlags = BufferUsage::None);
    // Creates a CPURead readback buffer, used for performing data readback from the GPU to the CPU.
    static BufferDesc CreateReadbackBufferDesc(std::string name,
                                               u64 byteSize,
                                               BufferUsage::Flags usageFlags = BufferUsage::None);
    // Creates a GPUOnly buffer useable as a StructuredBuffer or ByteAddressBuffer.
    static BufferDesc CreateGenericBufferDesc(std::string name, u64 byteSize, bool readWrite = false);
};

// Strongly defined type represents a buffer.
// We use a struct (instead of a typedef/using) to enforce compile-time correctness of handles.
struct BufferHandle : public Handle64<BufferHandle>
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
    u64 byteSize = GBufferWholeSize;

    u64 GetByteSize(const BufferDesc& desc) const;

    constexpr bool operator==(const BufferRegion&) const = default;

    static BufferRegion FullBuffer();
};

struct BufferCopyDesc
{
    u64 srcOffset = 0;
    u64 dstOffset = 0;
    u64 byteSize = GBufferWholeSize;

    u64 GetByteSize(const BufferDesc& desc) const;

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
