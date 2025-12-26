#pragma once

#include <Vex/Buffer.h>
#include <Vex/Utility/Hash.h>
#include <Vex/MemoryAllocation.h>
#include <Vex/Utility/NonNullPtr.h>
#include <Vex/Resource.h>
#include <Vex/Types.h>

#include <RHI/RHIBarrier.h>
#include <RHI/RHIFwd.h>

namespace vex
{
struct BufferBinding;

struct BufferViewDesc
{
    BufferBindingUsage usage;
    u32 strideByteSize;
    u64 offsetByteSize;
    u64 rangeByteSize;

    bool isAccelerationStructure = false;

    bool operator==(const BufferViewDesc&) const = default;

    u32 GetElementStride() const;
    u64 GetFirstElement() const;
    u64 GetElementCount() const;
};
} // namespace vex

// clang-format off
VEX_MAKE_HASHABLE(vex::BufferViewDesc, 
    VEX_HASH_COMBINE(seed, obj.usage);
    VEX_HASH_COMBINE(seed, obj.strideByteSize);
    VEX_HASH_COMBINE(seed, obj.offsetByteSize);
    VEX_HASH_COMBINE(seed, obj.rangeByteSize);
    VEX_HASH_COMBINE(seed, obj.isAccelerationStructure);
);
// clang-format on

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

    virtual BindlessHandle GetOrCreateBindlessView(const BufferBinding& binding, RHIDescriptorPool& descriptorPool);
    void FreeBindlessHandles(RHIDescriptorPool& descriptorPool);
    void FreeAllocation(RHIAllocator& allocator);

    [[nodiscard]] const BufferDesc& GetDesc() const
    {
        return desc;
    };

    [[nodiscard]] const Allocation& GetAllocation() const
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
    explicit RHIBufferBase(RHIAllocator& allocator, const BufferDesc& desc);

    virtual void AllocateBindlessHandle(RHIDescriptorPool& descriptorPool,
                                        BindlessHandle handle,
                                        const BufferViewDesc& desc) = 0;

    BufferViewDesc GetViewDescFromBinding(const BufferBinding& binding);

    BufferDesc desc;
    RHIBarrierSync lastSync = RHIBarrierSync::None;
    RHIBarrierAccess lastAccess = RHIBarrierAccess::NoAccess;

    NonNullPtr<RHIAllocator> allocator;
    Allocation allocation;

    std::unordered_map<BufferViewDesc, BindlessHandle> viewCache;
};

} // namespace vex