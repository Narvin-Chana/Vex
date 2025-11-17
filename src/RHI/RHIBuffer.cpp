#include "RHIBuffer.h"

#include <Vex/Bindings.h>
#include <Vex/RHIImpl/RHIAllocator.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>

namespace vex
{

RHIBufferBase::RHIBufferBase(RHIAllocator& allocator)
    : allocator{ allocator }
{
}

BindlessHandle RHIBufferBase::GetOrCreateBindlessView(const BufferBinding& binding, RHIDescriptorPool& descriptorPool)
{
    BufferViewDesc bufferView = GetViewDescFromBinding(binding);
    if (viewCache.contains(bufferView))
    {
        return viewCache[bufferView];
    }

    const BindlessHandle handle = descriptorPool.AllocateStaticDescriptor();

    AllocateBindlessHandle(descriptorPool, handle, bufferView);

    viewCache[bufferView] = handle;
    return handle;
}

void RHIBufferBase::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    for (auto [_, handle] : viewCache)
    {
        descriptorPool.FreeStaticDescriptor(handle);
    }
    viewCache.clear();
}

RHIBufferBase::RHIBufferBase(RHIAllocator& allocator, const BufferDesc& desc)
    : desc{ desc }
    , allocator{ allocator }
{
}

BufferViewDesc RHIBufferBase::GetViewDescFromBinding(const BufferBinding& binding)
{
    auto offset = binding.offsetByteSize.value_or(0);
    return { binding.usage,
             binding.strideByteSize.value_or(0),
             offset,
             binding.rangeByteSize.value_or(binding.buffer.desc.byteSize - offset) };
}

void RHIBufferBase::FreeAllocation(RHIAllocator& allocator)
{
#if VEX_USE_CUSTOM_ALLOCATOR_BUFFERS
    allocator.FreeResource(allocation);
#endif
}

u32 BufferViewDesc::GetElementStride() const noexcept
{
    switch (usage)
    {
    case BufferBindingUsage::StructuredBuffer:
    case BufferBindingUsage::RWStructuredBuffer:
        return strideByteSize;
    case BufferBindingUsage::ByteAddressBuffer:
    case BufferBindingUsage::RWByteAddressBuffer:
        return 4;
    default:
        break;
    }
    return 0;
}

u64 BufferViewDesc::GetFirstElement() const noexcept
{
    if (usage == BufferBindingUsage::ConstantBuffer)
    {
        return 0;
    }

    return offsetByteSize / GetElementStride();
}
u64 BufferViewDesc::GetElementCount() const noexcept
{
    if (usage == BufferBindingUsage::ConstantBuffer)
    {
        return 1;
    }

    return rangeByteSize / GetElementStride();
}
} // namespace vex