#pragma once

#include <Vex/Containers/FreeList.h>
#include <Vex/RHIFwd.h>
#include <Vex/Resource.h>

namespace vex
{

static constexpr u32 GDefaultDescriptorPoolSize = 65536;

enum class DescriptorType : u8
{
    Sampler,
    Texture,
    RWTexture,
    Buffer,
    RWBuffer,
};

class RHIDescriptorPoolBase
{
public:
    RHIDescriptorPoolBase();
    // Nullifies the passed in descriptor handle slot, to indicate that the resource is no longer useable.
    // We don't use BindlessHandle, as it is technically no longer valid.
    virtual void CopyNullDescriptor(u32 slotIndex) = 0;

    BindlessHandle AllocateStaticDescriptor();
    void FreeStaticDescriptor(BindlessHandle handle);

    BindlessHandle AllocateDynamicDescriptor();
    void FreeDynamicDescriptor(BindlessHandle handle);

    bool IsValid(BindlessHandle handle);

protected:
    struct BindlessAllocation
    {
        std::vector<u8> generations;
        FreeListAllocator handles;
    };
    BindlessAllocation allocator;
};

} // namespace vex
