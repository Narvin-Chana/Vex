#include "Buffer.h"

#include <Vex/Validation.h>

namespace vex
{

BufferRegion BufferRegion::Resolve(const BufferDesc& desc) const
{
    return BufferRegion{
        .offset = offset,
        .size = (size != GBufferWholeSize) ? size : desc.byteSize,
    };
}

namespace BufferUtil
{

void ValidateBufferDesc(const BufferDesc& desc)
{
    VEX_CHECK(!desc.name.empty(), "The buffer needs a name on creation.");
    VEX_CHECK(desc.byteSize != 0, "Buffer \"{}\" must have a size bigger than 0", desc.name)
}

void ValidateBufferCopyDesc(const BufferDesc& srcDesc, const BufferDesc& dstDesc, const BufferCopyDesc& copyDesc)
{
    ValidateBufferRegion(srcDesc, { copyDesc.srcOffset, copyDesc.size });
    ValidateBufferRegion(dstDesc, { copyDesc.dstOffset, copyDesc.size });
}

void ValidateBufferRegion(const BufferDesc& desc, const BufferRegion& region)
{
    VEX_CHECK(region.offset + region.size <= desc.byteSize,
              "Buffer region goes beyond buffer size: region subresource: {}, buffer size: {}",
              region.offset + region.size,
              desc.byteSize);
}

void ValidateSimpleBufferCopy(const BufferDesc& srcDesc, const BufferDesc& dstDesc)
{
    VEX_CHECK(srcDesc.byteSize <= dstDesc.byteSize,
              "Source buffer must fit in destination buffer for simple copy: Source size: {}, dest size: {}",
              srcDesc.byteSize,
              dstDesc.byteSize);
}

} // namespace BufferUtil

BufferCopyDesc BufferCopyDesc::Resolve(const BufferDesc& srcBuffer, const BufferDesc& dstBuffer) const
{
    VEX_CHECK(srcBuffer.byteSize == dstBuffer.byteSize,
              "Error resolving BufferCopyDesc for \"{}\" and \"{}\": Both buffers byteSizes should be equal!",
              srcBuffer.name,
              dstBuffer.name);
    return BufferCopyDesc{
        .srcOffset = srcOffset,
        .dstOffset = dstOffset,
        .size = srcBuffer.byteSize,
    };
}

} // namespace vex