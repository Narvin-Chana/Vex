#pragma once

#include <Vex/Buffer.h>
#include <Vex/MemoryAllocation.h>
#include <Vex/NonNullPtr.h>
#include <Vex/Resource.h>
#include <Vex/Types.h>

#include <RHI/RHIBarrier.h>
#include <RHI/RHIFwd.h>

namespace vex
{

class RHIBufferBase : public MappableResourceInterface
{
public:
    RHIBufferBase(RHIAllocator& allocator);
    RHIBufferBase(const RHIBufferBase&) = delete;
    RHIBufferBase& operator=(const RHIBufferBase&) = delete;
    RHIBufferBase(RHIBufferBase&&) = default;
    RHIBufferBase& operator=(RHIBufferBase&&) = default;
    ~RHIBufferBase() = default;

    // Raw direct access to buffer memory
    virtual BindlessHandle GetOrCreateBindlessView(BufferBindingUsage usage,
                                                   std::optional<u32> strideByteSize,
                                                   RHIDescriptorPool& descriptorPool) = 0;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) = 0;
    virtual void FreeAllocation(RHIAllocator& allocator) = 0;

    [[nodiscard]] const BufferDescription& GetDescription() const noexcept
    {
        return desc;
    };

    [[nodiscard]] const Allocation& GetAllocation() const noexcept
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

protected:
    explicit RHIBufferBase(RHIAllocator& allocator, const BufferDescription& desc);

    BufferDescription desc;
    RHIBarrierSync lastSync = RHIBarrierSync::None;
    RHIBarrierAccess lastAccess = RHIBarrierAccess::NoAccess;

    NonNullPtr<RHIAllocator> allocator;
    Allocation allocation;
};

} // namespace vex