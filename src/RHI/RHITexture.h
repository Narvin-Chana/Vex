#pragma once

#include <Vex/MemoryAllocation.h>
#include <Vex/Texture.h>

#include <RHI/RHIBarrier.h>
#include <RHI/RHIBindings.h>
#include <RHI/RHIFwd.h>

namespace vex
{

class RHITextureBase
{
public:
    RHITextureBase() = default;
    explicit RHITextureBase(RHIAllocator& allocator)
        : allocator{ &allocator } {};
    RHITextureBase(const RHITextureBase&) = delete;
    RHITextureBase& operator=(const RHITextureBase&) = delete;
    RHITextureBase(RHITextureBase&&) = default;
    RHITextureBase& operator=(RHITextureBase&&) = default;
    ~RHITextureBase() = default;

    virtual BindlessHandle GetOrCreateBindlessView(const TextureViewDesc& view, RHIDescriptorPool& descriptorPool) = 0;
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

protected:
    TextureDesc desc{};
    RHIAllocator* allocator{};
    Allocation allocation;
};

struct RHITextureState
{
    RHIBarrierSync sync = RHIBarrierSync::None;
    RHIBarrierAccess access = RHIBarrierAccess::NoAccess;
    RHITextureLayout layout = RHITextureLayout::Undefined;

    constexpr bool operator==(const RHITextureState&) const = default;
};

} // namespace vex