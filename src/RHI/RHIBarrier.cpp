#include "RHIBarrier.h"

#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>

namespace vex
{

RHIBufferBarrier::RHIBufferBarrier(NonNullPtr<RHIBuffer> buffer, RHIBarrierSync dstSync, RHIBarrierAccess dstAccess)
    : buffer(buffer)
    , dstSync(dstSync)
    , dstAccess(dstAccess)
{
}

RHITextureBarrier::RHITextureBarrier(NonNullPtr<RHITexture> texture,
                                     TextureSubresource subresource,
                                     RHIBarrierSync dstSync,
                                     RHIBarrierAccess dstAccess,
                                     RHITextureLayout dstLayout)
    : texture(texture)
    , subresource(subresource)
    , dstSync(dstSync)
    , dstAccess(dstAccess)
    , dstLayout(dstLayout)
{
}

} // namespace vex