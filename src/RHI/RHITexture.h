#pragma once

#include <vector>

#include <Vex/MemoryAllocation.h>
#include <Vex/QueueType.h>
#include <Vex/Resource.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>
#include <Vex/Utility/Validation.h>

#include <RHI/RHIBarrier.h>
#include <RHI/RHIFwd.h>

namespace vex
{

class RHITextureBase : public MappableResourceInterface
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

    virtual RHITextureBarrier GetClearTextureBarrier() = 0;

    const TextureDesc& GetDesc() const
    {
        return desc;
    }

    const Allocation& GetAllocation() const noexcept
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

    [[nodiscard]] RHIBarrierSync GetLastSyncForSubresource(u16 mip, u32 slice) const
    {
        VEX_CHECK(!IsLastBarrierStateUniform(), "Resource is in a uniform state, call GetLastSync instead.");
        return perSubresourceLastBarrierState[GetSubresourceIndex(mip, slice)].lastSync;
    }
    [[nodiscard]] RHIBarrierAccess GetLastAccessForSubresource(u16 mip, u32 slice) const
    {
        VEX_CHECK(!IsLastBarrierStateUniform(), "Resource is in a uniform state, call GetLastAccess instead.");
        return perSubresourceLastBarrierState[GetSubresourceIndex(mip, slice)].lastAccess;
    }
    [[nodiscard]] RHITextureLayout GetLastLayoutForSubresource(u16 mip, u32 slice) const
    {
        VEX_CHECK(!IsLastBarrierStateUniform(), "Resource is in a uniform state, call GetLastLayout instead.");
        return perSubresourceLastBarrierState[GetSubresourceIndex(mip, slice)].lastLayout;
    }

    void SetLastBarrierState(RHIBarrierSync sync, RHIBarrierAccess access, RHITextureLayout layout)
    {
        uniformLastBarrierState = { .lastSync = sync, .lastAccess = access, .lastLayout = layout };
        // Reset the per-subresource info (since we're using uniform last barrier state).
        perSubresourceLastBarrierState.clear();
    }

    void SetLastBarrierStateForSubresource(
        RHIBarrierSync sync, RHIBarrierAccess access, RHITextureLayout layout, u16 mip, u32 slice)
    {
        // Now update the subresource with the required last barrier state.
        perSubresourceLastBarrierState[GetSubresourceIndex(mip, slice)] = { sync, access, layout };
    }

    void EnsureLastBarrierStateNonUniform()
    {
        // If the perSubresourceLastBarrierState is already setup, we do nothing.
        if (!IsLastBarrierStateUniform())
        {
            return;
        }

        // If we still haven't allocated the perSubresource last barrier state vector, do it now!
        perSubresourceLastBarrierState.resize(desc.mips * desc.GetSliceCount());
        for (u16 mip = 0; mip < desc.mips; ++mip)
        {
            for (u32 slice = 0; slice < desc.GetSliceCount(); ++slice)
            {
                perSubresourceLastBarrierState[mip * desc.GetSliceCount() + slice] = *uniformLastBarrierState;
            }
        }
        // Reset the uniform last barrier state, so that future barriers are split per subresource.
        uniformLastBarrierState.reset();
    }

    bool IsLastBarrierStateUniform() const
    {
        return uniformLastBarrierState.has_value();
    }

protected:
    u32 GetSubresourceIndex(u16 mip, u32 slice) const
    {
        return static_cast<u32>(mip) + slice * desc.mips;
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