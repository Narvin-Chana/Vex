#include "RHIBuffer.h"

#include <Vex/RHIImpl/RHIBuffer.h>

namespace vex
{

RHIBufferBase::RHIBufferBase(RHIAllocator& allocator)
    : allocator{ allocator }
{
}

RHIBufferBase::RHIBufferBase(RHIAllocator& allocator, const BufferDescription& desc)
    : desc{ desc }
    , allocator{ allocator }
{
}

} // namespace vex