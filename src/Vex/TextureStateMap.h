#pragma once

#include <optional>
#include <tuple>
#include <vector>

#include <Vex/Texture.h>
#include <Vex/Types.h>

#include <RHI/RHITexture.h>

namespace vex
{

struct TextureStateMap
{
    RHITextureState Get(const TextureDesc& desc, const TextureSubresource& subresource) const;
    void Set(const TextureDesc& desc, const TextureSubresource& subresource, RHITextureState state);
    void SetUniform(RHITextureState state);

    bool IsUniform() const
    {
        return !perSubresourceState.has_value();
    }

    // Allows us to call the passed in func, with each function call representing a subresource section which has
    // uniform state. fn will always be called at least once.
    template <typename Fn>
    void ForEachStateSection(const TextureDesc& desc, const TextureSubresource& subresource, Fn&& fn) const
    {
        if (IsUniform())
        {
            fn(subresource, uniformState);
            return;
        }

        const u32 mipCount = desc.mips;
        const u32 sliceCount = desc.GetSliceCount();
        const u32 planeCount = desc.GetPlaneCount();

        std::optional<RHITextureState> sectionState;

        u32 sectionIdxStart = 0;
        // Inclusive end.
        u32 sectionIdxEnd = 0;

        auto IndexToMSP = [mipCount, sliceCount, planeCount](u32 idx) -> std::tuple<u16, u32, u32>
        {
            u32 plane = idx / (mipCount * sliceCount);
            u32 rem = idx % (mipCount * sliceCount);
            // index = mip + slice * mips  →  slice = rem / mips, mip = rem % mips
            u32 slice = rem / mipCount;
            u16 mip = static_cast<u16>(rem % mipCount);
            return std::tie(mip, slice, plane);
        };

        auto EmitSection = [&]()
        {
            if (!sectionState)
            {
                return;
            }

            auto [mipS, sliceS, planeS] = IndexToMSP(sectionIdxStart);
            auto [mipE, sliceE, planeE] = IndexToMSP(sectionIdxEnd);

            TextureSubresource sub;
            sub.startMip = mipS;
            sub.mipCount = static_cast<u16>(mipE - mipS + 1);
            sub.startSlice = sliceS;
            sub.sliceCount = sliceE - sliceS + 1;
            sub.aspect = TextureUtil::PlaneStartCountToTextureAspect(desc.format, planeS, planeE + 1 - planeS);

            fn(sub, *sectionState);
            sectionState.reset();
        };

        TextureUtil::ForEachSubresourceIndices(subresource,
                                               desc,
                                               [&](u16 mip, u32 slice, u32 plane)
                                               {
                                                   const u32 idx = GetSubresourceIndex(desc, mip, slice, plane);
                                                   const RHITextureState state = (*perSubresourceState)[idx];

                                                   if (!sectionState || *sectionState != state)
                                                   {
                                                       EmitSection();
                                                       sectionState = state;
                                                       sectionIdxStart = idx;
                                                   }
                                                   sectionIdxEnd = idx;
                                               });

        // Emit the last section before exiting.
        EmitSection();
    }

    void DetectUniformity();

private:
    [[nodiscard]] u32 GetSubresourceIndex(const TextureDesc& desc, u16 mip, u32 slice, u32 plane) const;
    
    RHITextureState uniformState;
    std::optional<std::vector<RHITextureState>> perSubresourceState;
};

} // namespace vex