#include "Buffer.h"

#include <Vex/Validation.h>

namespace vex
{

namespace BufferUtil
{

void ValidateBufferDesc(const BufferDesc& desc)
{
    VEX_CHECK(!desc.name.empty(), "The buffer needs a name on creation.");
    VEX_CHECK(desc.byteSize != 0, "Buffer \"{}\" must have a size greater than 0", desc.name)
}

void ValidateBufferCopyDesc(const BufferDesc& srcDesc, const BufferDesc& dstDesc, const BufferCopyDesc& copyDesc)
{
    VEX_CHECK(srcDesc.byteSize == dstDesc.byteSize,
              "Error validating BufferCopyDesc for \"{}\" and \"{}\": Both buffers' byteSizes should be equal "
              "(currently {} bytes vs {} bytes)!",
              srcDesc.name,
              dstDesc.name,
              srcDesc.byteSize,
              dstDesc.byteSize);

    ValidateBufferRegion(srcDesc, { copyDesc.srcOffset, copyDesc.byteSize });
    ValidateBufferRegion(dstDesc, { copyDesc.dstOffset, copyDesc.byteSize });
}

void ValidateBufferRegion(const BufferDesc& desc, const BufferRegion& region)
{
    VEX_CHECK(region.offset < desc.byteSize,
              "Invalid region for resource \"{}\": The buffer's offset ({}) cannot be larger than the "
              "actual buffer's byte size ({}).",
              desc.name,
              region.offset,
              desc.byteSize);

    if (region.byteSize != GBufferWholeSize)
    {
        VEX_CHECK(region.offset + region.GetByteSize(desc) <= desc.byteSize,
                  "Invalid region for resource \"{}\": The region accesses more bytes than available, "
                  "region offset: {}, region byteSize: {}, "
                  " buffer byteSize {}",
                  desc.name,
                  region.offset,
                  region.GetByteSize(desc),
                  desc.byteSize);
    }
}

void ValidateSimpleBufferCopy(const BufferDesc& srcDesc, const BufferDesc& dstDesc)
{
    VEX_CHECK(srcDesc.byteSize <= dstDesc.byteSize,
              "Source buffer must fit in destination buffer for simple copy: Source size: {}, Dest size: {}",
              srcDesc.byteSize,
              dstDesc.byteSize);
}

} // namespace BufferUtil

BufferDesc BufferDesc::CreateUniformBufferDesc(std::string name, u64 byteSize)
{
    return {
        .name = std::move(name),
        .byteSize = byteSize,
        .usage = BufferUsage::UniformBuffer,
        .memoryLocality = ResourceMemoryLocality::CPUWrite,
    };
}

BufferDesc BufferDesc::CreateVertexBufferDesc(std::string name, u64 byteSize, bool allowShaderRead)
{
    BufferUsage::Flags usageFlags = BufferUsage::VertexBuffer;
    if (allowShaderRead)
    {
        usageFlags |= BufferUsage::GenericBuffer;
    }
    return {
        .name = std::move(name),
        .byteSize = byteSize,
        .usage = usageFlags,
        .memoryLocality = ResourceMemoryLocality::GPUOnly,
    };
}

BufferDesc BufferDesc::CreateIndexBufferDesc(std::string name, u64 byteSize, bool allowShaderRead)
{
    BufferUsage::Flags usageFlags = BufferUsage::IndexBuffer;
    if (allowShaderRead)
    {
        usageFlags |= BufferUsage::GenericBuffer;
    }
    return {
        .name = std::move(name),
        .byteSize = byteSize,
        .usage = usageFlags,
        .memoryLocality = ResourceMemoryLocality::GPUOnly,
    };
}

BufferDesc BufferDesc::CreateStagingBufferDesc(std::string name, u64 byteSize, BufferUsage::Flags usageFlags)
{
    return {
        .name = std::move(name),
        .byteSize = byteSize,
        .usage = usageFlags,
        .memoryLocality = ResourceMemoryLocality::CPUWrite,
    };
}

BufferDesc BufferDesc::CreateReadbackBufferDesc(std::string name, u64 byteSize, BufferUsage::Flags usageFlags)
{
    return {
        .name = std::move(name),
        .byteSize = byteSize,
        .usage = usageFlags,
        .memoryLocality = ResourceMemoryLocality::CPURead,
    };
}

u64 BufferRegion::GetByteSize(const BufferDesc& desc) const
{
    return byteSize == GBufferWholeSize ? desc.byteSize : byteSize;
}

BufferRegion BufferRegion::FullBuffer()
{
    // Default value means full buffer.
    return BufferRegion{};
}

u64 BufferCopyDesc::GetByteSize(const BufferDesc& desc) const
{
    return byteSize == GBufferWholeSize ? desc.byteSize : byteSize;
}

} // namespace vex