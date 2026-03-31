#pragma once

#include <Vex/Containers/FreeList.h>
#include <Vex/Resource.h>
#include <Vex/TextureSampler.h>

namespace vex
{

static constexpr u32 GDefaultDescriptorPoolSize = 65536;

enum class DescriptorType : u8
{
    Resource,
    Sampler,
};

class RHIDescriptorPoolBase
{
public:
    RHIDescriptorPoolBase();

    virtual BindlessHandle CreateBindlessSampler(const TextureSampler& textureSampler) = 0;
    virtual void FreeBindlessSampler(BindlessHandle handle) = 0;

    // Nullifies the passed in descriptor handle slot, to indicate that the resource is no longer usable.
    // We don't use BindlessHandle, as it is technically no longer valid.
    virtual void CopyNullDescriptor(DescriptorType descriptorType, u32 slotIndex) = 0;

    BindlessHandle AllocateStaticDescriptor(DescriptorType descriptorType);
    void FreeStaticDescriptor(DescriptorType descriptorType, BindlessHandle handle);

    bool IsValid(BindlessHandle handle);

protected:
    struct BindlessAllocation
    {
        std::vector<u8> generations;
        FreeListAllocator32 handles;
    };
    BindlessAllocation allocator;
    BindlessAllocation samplerAllocator;
};

} // namespace vex
