#include "TextureStateMap.h"

#include <algorithm>

#include <Vex/Platform/Debug.h>

namespace vex
{

RHITextureState TextureStateMap::Get(const TextureDesc& desc, const TextureSubresource& subresource) const
{
    if (IsUniform())
    {
        return uniformState;
    }

    std::optional<RHITextureState> state;
    TextureUtil::ForEachSubresourceIndices(
        subresource,
        desc,
        [this, &desc, &state](u16 mip, u32 slice, u32 plane)
        {
            const u32 key = GetSubresourceIndex(desc, mip, slice, plane);
            const RHITextureState subresourceState = (*perSubresourceState)[key];
            if (!state)
            {
                state = subresourceState;
            }
            else
            {
                VEX_ASSERT(*state == subresourceState, "Subresource parts are not all in the same layout");
            }
        });

    return state.value();
}

void TextureStateMap::Set(const TextureDesc& desc, const TextureSubresource& subresource, RHITextureState newState)
{
    if (subresource.IsFullResource(desc))
    {
        SetUniform(newState);
        return;
    }

    if (IsUniform())
    {
        perSubresourceState.emplace();
        perSubresourceState->resize(desc.mips * desc.GetSliceCount() * desc.GetPlaneCount());
        for (u16 mip = 0; mip < desc.mips; ++mip)
        {
            for (u32 slice = 0; slice < desc.GetSliceCount(); ++slice)
            {
                for (u32 plane = 0; plane < desc.GetPlaneCount(); ++plane)
                {
                    (*perSubresourceState)[GetSubresourceIndex(desc, mip, slice, plane)] = uniformState;
                }
            }
        }
    }

    TextureUtil::ForEachSubresourceIndices(subresource,
                                           desc,
                                           [this, &desc, &newState](u16 mip, u32 slice, u32 plane)
                                           {
                                               const u32 key = GetSubresourceIndex(desc, mip, slice, plane);
                                               (*perSubresourceState)[key] = newState;
                                           });

    // Check if the state can be made uniform now that we've modified a section of the perSubresourceState.
    DetectUniformity();
}

void TextureStateMap::SetUniform(RHITextureState newState)
{
    uniformState = newState;
    perSubresourceState.reset();
}

void TextureStateMap::DetectUniformity()
{
    // Already uniform.
    if (IsUniform())
    {
        return;
    }

    const RHITextureState firstState = perSubresourceState->front();
    const bool allSame = std::all_of(perSubresourceState->begin(),
                                     perSubresourceState->end(),
                                     [&firstState](const RHITextureState& state) { return state == firstState; });

    if (allSame)
    {
        // Uniformity detected!
        SetUniform(firstState);
    }
}

u32 TextureStateMap::GetSubresourceIndex(const TextureDesc& desc, u16 mip, u32 slice, u32 plane) const
{
    VEX_ASSERT(mip < desc.mips && slice < desc.GetSliceCount() && plane < desc.GetPlaneCount());
    return plane * (desc.mips * desc.GetSliceCount()) + static_cast<u32>(mip) + slice * desc.mips;
}

} // namespace vex