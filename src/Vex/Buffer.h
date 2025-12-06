#pragma once

#include <string>

#include <Vex/Logger.h>
#include <Vex/Resource.h>
#include <Vex/Types.h>
#include <Vex/Utility/EnumFlags.h>
#include <Vex/Utility/Handle.h>

namespace vex
{

// clang-format off

// Determines how the buffer can be used.
BEGIN_VEX_ENUM_FLAGS(BufferUsage, u8)
    None                            = 0,      // Buffers that will never be bound anywhere. Mostly used for staging buffers.
    GenericBuffer                   = 1 << 0, // Buffers that can be read from shaders (SRV)
    UniformBuffer                   = 1 << 1, // Buffers with specific alignment constraints uniformly read across waves (CBV)
    ReadWriteBuffer                 = 1 << 2, // Buffers with read and write operations in shaders (UAV)
    VertexBuffer                    = 1 << 3, // Buffers used for vertex buffers
    IndexBuffer                     = 1 << 4, // Buffers used for index buffers
    IndirectArgs                    = 1 << 5, // Buffers used as parameters for an indirect dispatch.
    RaytracingAccelerationStructure = 1 << 6, // Buffers used as an RT Acceleration Structure.
END_VEX_ENUM_FLAGS();

// clang-format on

// Defines what the specific binding will bind as maps directly to the type that will be used in the shader to access
// the buffer.
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

    // Helpers to create a buffer description.

    // Creates a CPUWrite buffer useable as a Uniform (/constant) Buffer.
    static BufferDesc CreateUniformBufferDesc(std::string name, u64 byteSize);
    // Creates a GPUOnly buffer useable as an Vertex Buffer.
    static BufferDesc CreateVertexBufferDesc(std::string name, u64 byteSize, bool allowShaderRead = false);
    // Creates a GPUOnly buffer useable as an Index Buffer.
    static BufferDesc CreateIndexBufferDesc(std::string name, u64 byteSize, bool allowShaderRead = false);
    // Creates a CPUWrite staging buffer useful for uploading data to GPUOnly resources.
    static BufferDesc CreateStagingBufferDesc(std::string name,
                                              u64 byteSize,
                                              BufferUsage::Flags usageFlags = BufferUsage::None);
    // Creates a CPURead readback buffer, used for performing data readback from the GPU to the CPU.
    static BufferDesc CreateReadbackBufferDesc(std::string name,
                                               u64 byteSize,
                                               BufferUsage::Flags usageFlags = BufferUsage::None);
    // Creates a GPUOnly buffer useable as a Structured Buffer.
    static BufferDesc CreateStructuredBufferDesc(std::string name, u64 byteSize);
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
    u64 srcOffset;
    u64 dstOffset;
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
