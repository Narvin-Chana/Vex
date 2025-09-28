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
              "Source buffer must fit in destination buffer for simple copy: Source size: {}, Dest size: {}",
              srcDesc.byteSize,
              dstDesc.byteSize);
}

} // namespace BufferUtil

} // namespace vex