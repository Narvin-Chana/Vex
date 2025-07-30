#pragma once

#include <string>

#include <Vex/EnumFlags.h>
#include <Vex/Handle.h>
#include <Vex/Types.h>

namespace vex
{

// clang-format off

// Determines how the buffer can be used.
BEGIN_VEX_ENUM_FLAGS(BufferUsage, u8)
    None                            = 0,
    ShaderRead                      = 1 << 0, // SRV in DX12, read StorageBuffer in Vulkan
    ShaderReadWrite                 = 1 << 1, // UAV in DX12, readWrite StorageBuffer in Vulkan
    VertexBuffer                    = 1 << 2,
    IndexBuffer                     = 1 << 3,
    IndirectArgs                    = 1 << 4,
    RaytracingAccelerationStructure = 1 << 5,
    CPUVisible                      = 1 << 6, // Can be read by CPU
    CPUWrite                        = 1 << 7, // Can be written to by CPU
    // GPU visiblity is deduced from other flags.
END_VEX_ENUM_FLAGS();

// clang-format on

struct BufferDescription
{
    std::string name;
    u32 byteSize = 0;
    // Stride of zero means this is a raw buffer (unstructured, represented in shaders as a ByteAddressBuffer)
    u32 stride = 0;
    BufferUsage::Flags usage = BufferUsage::None;

    bool IsStructured() const
    {
        return stride != 0;
    }
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

} // namespace vex
