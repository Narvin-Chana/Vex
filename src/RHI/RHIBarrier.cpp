#include "RHIBarrier.h"

#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>

namespace vex
{

RHIBufferBarrier::RHIBufferBarrier(NonNullPtr<RHIBuffer> buffer, RHIBarrierSync dstSync, RHIBarrierAccess dstAccess)
    : buffer(buffer)
    , srcSync(buffer->GetLastSync())
    , srcAccess(buffer->GetLastAccess())
    , dstSync(dstSync)
    , dstAccess(dstAccess)
{
}

RHITextureBarrier::RHITextureBarrier(NonNullPtr<RHITexture> texture,
                                     RHIBarrierSync dstSync,
                                     RHIBarrierAccess dstAccess,
                                     RHITextureLayout dstLayout)
    : texture(texture)
    , srcSync(texture->GetLastSync())
    , srcAccess(texture->GetLastAccess())
    , srcLayout(texture->GetLastLayout())
    , dstSync(dstSync)
    , dstAccess(dstAccess)
    , dstLayout(dstLayout)
{
}

} // namespace vex