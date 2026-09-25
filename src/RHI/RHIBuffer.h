#pragma once

#include <Vex/Buffer.h>
#include <Vex/MemoryAllocation.h>
#include <Vex/Types.h>
#include <Vex/Utility/Hash.h>
#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIBindings.h>
#include <RHI/RHIFwd.h>

namespace vex
{

class RHIBufferBase
{
public:
    explicit RHIBufferBase(RHIAllocator& allocator);
    RHIBufferBase(const RHIBufferBase&) = delete;
    RHIBufferBase& operator=(const RHIBufferBase&) = delete;
    RHIBufferBase(RHIBufferBase&&) = default;
    RHIBufferBase& operator=(RHIBufferBase&&) = default;
    ~RHIBufferBase() = default;

    bool IsMappable() const;
    Span<byte> GetMappedData() const
    {
        if (!IsMappable())
        {
            VEX_LOG(Warning, "Attempting to access mapped data on an non-mappable buffer...");
        }
        return mappedData;
    }

    virtual BindlessHandle GetOrCreateBindlessView(const BufferViewDesc& view, RHIDescriptorPool& descriptorPool);
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

protected:
    explicit RHIBufferBase(RHIAllocator& allocator, const BufferDesc& desc);

    virtual void AllocateBindlessHandle(RHIDescriptorPool& descriptorPool,
                                        BindlessHandle handle,
                                        const BufferViewDesc& desc) = 0;

    BufferDesc desc;

    NonNullPtr<RHIAllocator> allocator;
    Allocation allocation;

    std::span<byte> mappedData;

    std::unordered_map<BufferViewDesc, BindlessHandle> viewCache;
};

} // namespace vex