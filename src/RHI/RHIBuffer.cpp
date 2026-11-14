#include "RHIBuffer.h"

#include <ranges>

#include <Vex/Bindings.h>
#include <Vex/RHIImpl/RHIAllocator.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>

namespace vex
{

RHIBufferBase::RHIBufferBase(RHIAllocator& allocator)
    : allocator{ allocator }
{
}

bool RHIBufferBase::IsMappable() const
{
    return desc.memoryLocality == ResourceMemoryLocality::CPURead ||
           desc.memoryLocality == ResourceMemoryLocality::CPUWrite;
}

BindlessHandle RHIBufferBase::GetOrCreateBindlessView(const BufferViewDesc& view, RHIDescriptorPool& descriptorPool)
{
    auto [it, inserted] = viewCache.try_emplace(view);
    if (inserted)
    {
        it->second = descriptorPool.AllocateStaticDescriptor(DescriptorType::Resource);
        AllocateBindlessHandle(descriptorPool, it->second, view);
    }
    return it->second;
}

void RHIBufferBase::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    for (const auto handle : viewCache | std::views::values)
    {
        descriptorPool.FreeStaticDescriptor(DescriptorType::Resource, handle);
    }
    viewCache.clear();
}

RHIBufferBase::RHIBufferBase(RHIAllocator& allocator, const BufferDesc& desc)
    : desc{ desc }
    , allocator{ allocator }
{
}

void RHIBufferBase::FreeAllocation(RHIAllocator& allocator)
{
#if VEX_USE_CUSTOM_RESOURCE_ALLOCATOR
    allocator.FreeResource(allocation);
#endif
}

} // namespace vex