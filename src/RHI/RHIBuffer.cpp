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
    if (viewCache.contains(view))
    {
        return viewCache[view];
    }

    const BindlessHandle handle = descriptorPool.AllocateStaticDescriptor(DescriptorType::Resource);

    AllocateBindlessHandle(descriptorPool, handle, view);

    viewCache[view] = handle;
    return handle;
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