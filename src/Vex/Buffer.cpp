#include "Buffer.h"

#include <Vex/Validation.h>

namespace vex
{

namespace BufferUtil
{
void ValidateBufferDescription(const BufferDescription& desc)
{
    VEX_CHECK(!desc.name.empty(), "The buffer needs a name on creation.");
    VEX_CHECK(desc.byteSize != 0, "Buffer \"{}\" must have a size bigger than 0", desc.name)
}

void ValidateBufferCopyDescription(const BufferDescription& srcDesc,
                                   const BufferDescription& dstDesc,
                                   const BufferCopyDescription& copyDesc)
{
    ValidateBufferSubresource(srcDesc, { copyDesc.srcOffset, copyDesc.size });
    ValidateBufferSubresource(dstDesc, { copyDesc.dstOffset, copyDesc.size });
}

void ValidateBufferSubresource(const BufferDescription& desc, const BufferSubresource& subresource)
{
    VEX_CHECK(subresource.offset + subresource.size <= desc.byteSize,
              "Buffer subresource goes beyond buffer size: subresource range: {}, buffer size: {}",
              subresource.offset + subresource.size,
              desc.byteSize);
}

void ValidateSimpleBufferCopy(const BufferDescription& srcDesc, const BufferDescription& dstDesc)
{
    VEX_CHECK(srcDesc.byteSize <= dstDesc.byteSize,
              "Source buffer must fit in destination buffer for simple copy: Source size: {}, dest size: {}",
              srcDesc.byteSize,
              dstDesc.byteSize);
}

} // namespace BufferUtil

} // namespace vex