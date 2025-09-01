#include "Buffer.h"

#include <Vex/Validation.h>

namespace vex
{

namespace BufferUtil
{
void ValidateBufferDescription(const BufferDescription& desc)
{
    if (desc.name.empty())
    {
        LogFailValidation("The buffer needs a name on creation.");
    }

    if (desc.byteSize == 0)
    {
        LogFailValidation("Buffer \"{}\" must have a size bigger than 0", desc.name);
    }
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
    if (subresource.offset + subresource.size > desc.byteSize)
    {
        LogFailValidation("Buffer subresource goes beyond buffer size: subresource range: {}, buffer size: {}",
                          subresource.offset + subresource.size,
                          desc.byteSize);
    }
}

void ValidateSimpleBufferCopy(const BufferDescription& srcDesc, const BufferDescription& dstDesc)
{
    if (srcDesc.byteSize > dstDesc.byteSize)
    {
        LogFailValidation(
            "Source buffer must fit in destination buffer for simple copy: Source size: {}, dest size: {}",
            srcDesc.byteSize,
            dstDesc.byteSize);
    }
}

} // namespace BufferUtil

} // namespace vex