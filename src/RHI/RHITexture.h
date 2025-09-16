#pragma once

#include <Vex/CommandQueueType.h>
#include <Vex/Logger.h>
#include <Vex/MemoryAllocation.h>
#include <Vex/Resource.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>

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

    const TextureDescription& GetDescription() const
    {
        return description;
    }

    const Allocation& GetAllocation() const noexcept
    {
        return allocation;
    }

    [[nodiscard]] RHIBarrierSync GetLastSync() const
    {
        return lastSync;
    }
    void SetLastSync(RHIBarrierSync sync)
    {
        lastSync = sync;
    }

    [[nodiscard]] RHIBarrierAccess GetLastAccess() const
    {
        return lastAccess;
    }
    void SetLastAccess(RHIBarrierAccess access)
    {
        lastAccess = access;
    }

    [[nodiscard]] RHITextureLayout GetLastLayout() const
    {
        return lastLayout;
    }
    void SetLastLayout(RHITextureLayout layout)
    {
        lastLayout = layout;
    }

protected:
    TextureDescription description;
    RHIBarrierSync lastSync = RHIBarrierSync::None;
    RHIBarrierAccess lastAccess = RHIBarrierAccess::NoAccess;
    RHITextureLayout lastLayout = RHITextureLayout::Undefined;

    RHIAllocator* allocator{};
    Allocation allocation;
};

} // namespace vex