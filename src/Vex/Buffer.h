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

struct BufferDescription
{
    std::string name;
    u64 byteSize = 0;
    BufferUsage::Flags usage = BufferUsage::GenericBuffer;
    ResourceMemoryLocality memoryLocality = ResourceMemoryLocality::GPUOnly;

    // Helpers to create a buffer description.

    // Creates a CPUWrite buffer useable as a Uniform (/constant) Buffer.
    static BufferDescription CreateUniformBufferDesc(std::string name, u64 byteSize);
    // Creates a GPUOnly buffer useable as an Vertex Buffer.
    static BufferDescription CreateVertexBufferDesc(std::string name, u64 byteSize, bool allowShaderRead = false);
    // Creates a GPUOnly buffer useable as an Index Buffer.
    static BufferDescription CreateIndexBufferDesc(std::string name, u64 byteSize, bool allowShaderRead = false);
    // Creates a CPUWrite staging buffer useful for uploading data to GPUOnly resources.
    static BufferDescription CreateStagingBufferDesc(std::string name,
                                                     u64 byteSize,
                                                     BufferUsage::Flags usageFlags = BufferUsage::None);
    // Creates a CPURead readback buffer, used for performing data readback from the GPU to the CPU.
    static BufferDescription CreateReadbackBufferDesc(std::string name,
                                                      u64 byteSize,
                                                      BufferUsage::Flags usageFlags = BufferUsage::None);
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
    BufferDescription description;
};

struct BufferSubresource
{
    u64 offset;
    u64 size;
};

struct BufferCopyDescription
{
    u64 srcOffset;
    u64 dstOffset;
    u64 size;
};

namespace BufferUtil
{
void ValidateBufferDescription(const BufferDescription& desc);
void ValidateBufferCopyDescription(const BufferDescription& srcDesc,
                                   const BufferDescription& dstDesc,
                                   const BufferCopyDescription& copyDesc);
void ValidateBufferSubresource(const BufferDescription& desc, const BufferSubresource& subresource);
void ValidateSimpleBufferCopy(const BufferDescription& srcDesc, const BufferDescription& dstDesc);

} // namespace BufferUtil

} // namespace vex
