#pragma once

#include <vector>

#include <Vex/MemoryAllocation.h>
#include <Vex/QueueType.h>
#include <Vex/Resource.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>
#include <Vex/Utility/Validation.h>

#include <RHI/RHIBarrier.h>
#include <RHI/RHIBindings.h>
#include <RHI/RHIFwd.h>

namespace vex
{
struct RHITextureBinding;

class RHITextureBase
{
public:
    RHITextureBase() = default;
    RHITextureBase(RHIAllocator& allocator)
        : allocator{ &allocator } {};
    RHITextureBase(const RHITextureBase&) = delete;
    RHITextureBase& operator=(const RHITextureBase&) = delete;
    RHITextureBase(RHITextureBase&&) = default;
    RHITextureBase& operator=(RHITextureBase&&) = default;
    ~RHITextureBase() = default;

    virtual BindlessHandle GetOrCreateBindlessView(const TextureBinding& binding,
                                                   RHIDescriptorPool& descriptorPool) = 0;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) = 0;
    virtual void FreeAllocation(RHIAllocator& allocator) = 0;

    [[nodiscard]] const TextureDesc& GetDesc() const
    {
        return desc;
    }

    [[nodiscard]] const Allocation& GetAllocation() const
    {
        return allocation;
    }

    [[nodiscard]] RHIBarrierSync GetLastSync() const
    {
        VEX_CHECK(IsLastBarrierStateUniform(),
                  "Resource is in a non-uniform state, call GetLastSyncForSubresource instead.");
        return uniformLastBarrierState->lastSync;
    }
    [[nodiscard]] RHIBarrierAccess GetLastAccess() const
    {
        VEX_CHECK(IsLastBarrierStateUniform(),
                  "Resource is in a non-uniform state, call GetLastAccessForSubresource instead.");
        return uniformLastBarrierState->lastAccess;
    }
    [[nodiscard]] RHITextureLayout GetLastLayout() const
    {
        VEX_CHECK(IsLastBarrierStateUniform(),
                  "Resource is in a non-uniform state, call GetLastLayoutForSubresource instead.");
        return uniformLastBarrierState->lastLayout;
    }

    [[nodiscard]] RHITextureLayout GetLastLayoutForSubresource(const TextureSubresource& subresource) const
    {
        if (IsLastBarrierStateUniform())
            return GetLastLayout();

        std::optional<RHITextureLayout> layout;

        TextureUtil::ForEachSubresourceIndices(
            subresource,
            GetDesc(),
            [this, &layout](u32 mip, u32 slice, u32 plane)
            {
                RHITextureLayout resourceLayout = GetLastLayoutForSubresource(mip, slice, plane);
                if (!layout)
                {
                    layout = resourceLayout;
                }
                else
                {
                    VEX_ASSERT(*layout == resourceLayout, "Subresource parts are not all in the same layout");
                }
            });

        return *layout;
    }

    [[nodiscard]] RHIBarrierSync GetLastSyncForSubresource(u16 mip, u32 slice, u32 plane) const
    {
        if (IsLastBarrierStateUniform())
            return GetLastSync();
        return perSubresourceLastBarrierState[GetSubresourceIndex(mip, slice, plane)].lastSync;
    }
    [[nodiscard]] RHIBarrierAccess GetLastAccessForSubresource(u16 mip, u32 slice, u32 plane) const
    {
        if (IsLastBarrierStateUniform())
            return GetLastAccess();
        return perSubresourceLastBarrierState[GetSubresourceIndex(mip, slice, plane)].lastAccess;
    }
    [[nodiscard]] RHITextureLayout GetLastLayoutForSubresource(u16 mip, u32 slice, u32 plane) const
    {
        if (IsLastBarrierStateUniform())
            return GetLastLayout();
        return perSubresourceLastBarrierState[GetSubresourceIndex(mip, slice, plane)].lastLayout;
    }

    void SetLastBarrierState(RHIBarrierSync sync, RHIBarrierAccess access, RHITextureLayout layout)
    {
        uniformLastBarrierState = { .lastSync = sync, .lastAccess = access, .lastLayout = layout };
        // Reset the per-subresource info (since we're using uniform last barrier state).
        perSubresourceLastBarrierState.clear();
    }

    void SetLastBarrierStateForSubresource(
        RHIBarrierSync sync, RHIBarrierAccess access, RHITextureLayout layout, u16 mip, u32 slice, u32 plane)
    {
        // Now update the subresource with the required last barrier state.
        perSubresourceLastBarrierState[GetSubresourceIndex(mip, slice, plane)] = { sync, access, layout };
    }

    void EnsureLastBarrierStateNonUniform()
    {
        // If the perSubresourceLastBarrierState is already setup, we do nothing.
        if (!IsLastBarrierStateUniform())
        {
            return;
        }

        // If we still haven't allocated the perSubresource last barrier state vector, do it now!
        perSubresourceLastBarrierState.resize(desc.mips * desc.GetSliceCount() * desc.GetPlaneCount());
        for (u16 mip = 0; mip < desc.mips; ++mip)
        {
            for (u32 slice = 0; slice < desc.GetSliceCount(); ++slice)
            {
                for (u32 plane = 0; plane < desc.GetPlaneCount(); ++plane)
                {
                    perSubresourceLastBarrierState[GetSubresourceIndex(mip, slice, plane)] = *uniformLastBarrierState;
                }
            }
        }
        // Reset the uniform last barrier state, so that future barriers are split per subresource.
        uniformLastBarrierState.reset();
    }

    [[nodiscard]] bool IsLastBarrierStateUniform() const
    {
        return uniformLastBarrierState.has_value();
    }

protected:
    [[nodiscard]] u32 GetSubresourceIndex(u16 mip, u32 slice, u32 plane) const
    {
        VEX_ASSERT(mip < desc.mips && slice < desc.GetSliceCount() && plane < desc.GetPlaneCount());
        return plane * (desc.mips * desc.GetSliceCount()) + static_cast<u32>(mip) + slice * desc.mips;
    }

    TextureDesc desc;

    struct LastBarrierState
    {
        RHIBarrierSync lastSync = RHIBarrierSync::None;
        RHIBarrierAccess lastAccess = RHIBarrierAccess::NoAccess;
        RHITextureLayout lastLayout = RHITextureLayout::Undefined;
    };

    // Fast path for if the resource has a fully uniform state.
    std::optional<LastBarrierState> uniformLastBarrierState = LastBarrierState{};
    // Slower path for if the resource has a non-uniform state.
    std::vector<LastBarrierState> perSubresourceLastBarrierState;

    RHIAllocator* allocator{};
    Allocation allocation;
};

} // namespace vex