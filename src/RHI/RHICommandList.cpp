#include "RHICommandList.h"

#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/RHIImpl/RHITimestampQueryPool.h>
#include <Vex/Utility/ByteUtils.h>
#include <Vex/Utility/Validation.h>

namespace vex
{

void RHICommandListBase::Open()
{
    if (isOpen)
    {
        VEX_LOG(Fatal, "Attempting to open an already open command list.");
        return;
    }

    isOpen = true;
}

void RHICommandListBase::Close()
{
    if (!isOpen)
    {
        VEX_LOG(Fatal, "Attempting to close an already closed command list.");
        return;
    }

    if (!queries.empty())
    {
        queryPool->FetchQueriesTimestamps(reinterpret_cast<RHICommandList&>(*this), queries);
    }

    isOpen = false;
}

void RHICommandListBase::BufferBarrier(RHIBuffer& buffer, RHIBarrierSync sync, RHIBarrierAccess access)
{
    RHIBufferBarrier barrier{ buffer, sync, access };
    Barrier({ &barrier, 1 }, {});
}

void RHICommandListBase::TextureBarrier(RHITexture& texture,
                                        RHIBarrierSync sync,
                                        RHIBarrierAccess access,
                                        RHITextureLayout layout)
{
    RHITextureBarrier barrier{ texture, TextureSubresource{}, sync, access, layout };
    Barrier({}, { &barrier, 1 });
}

void RHICommandListBase::Copy(RHITexture& src, RHITexture& dst)
{
    TextureCopyDesc copyDesc;
    Copy(src, dst, std::span(&copyDesc, 1));
}

void RHICommandListBase::Copy(RHIBuffer& src, RHIBuffer& dst)
{
    Copy(src, dst, BufferCopyDesc{ 0, 0, src.GetDesc().byteSize });
}

void RHICommandListBase::Copy(RHIBuffer& src, RHITexture& dst)
{
    std::vector<BufferTextureCopyDesc> bufferToTextureCopies = BufferTextureCopyDesc::AllMips(dst.GetDesc());

    for (const auto& copy : bufferToTextureCopies)
    {
        TextureCopyUtil::ValidateBufferTextureCopyDesc(src.GetDesc(), dst.GetDesc(), copy);
    }

    Copy(src, dst, bufferToTextureCopies);
}

void RHICommandListBase::Copy(RHITexture& src, RHIBuffer& dst)
{
    std::vector<BufferTextureCopyDesc> bufferToTextureCopies = BufferTextureCopyDesc::AllMips(src.GetDesc());

    for (const auto& copy : bufferToTextureCopies)
    {
        TextureCopyUtil::ValidateBufferTextureCopyDesc(dst.GetDesc(), src.GetDesc(), copy);
    }

    Copy(src, dst, bufferToTextureCopies);
}

void RHICommandListBase::SetSyncTokens(Span<const SyncToken> tokens)
{
    syncTokens = { tokens.begin(), tokens.end() };
}

void RHICommandListBase::UpdateTimestampQueryTokens(SyncToken token)
{
    if (queryPool)
    {
        queryPool->UpdateSyncTokens(token, queries);
    }
    queries.clear();
}

} // namespace vex