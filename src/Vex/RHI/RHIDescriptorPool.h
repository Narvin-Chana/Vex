#pragma once

#include "Vex/Hash.h"

#include <Vex/Containers/FreeList.h>
#include <Vex/Handle.h>
#include <Vex/RHI/RHIFwd.h>
#include <Vex/Resource.h>

namespace vex
{

using BindlessHandle = Handle<>;

static constexpr BindlessHandle GInvalidBindlessHandle;

class RHIDescriptorPool
{
public:
    virtual ~RHIDescriptorPool() = default;
    // Static descriptors are persistent beyond a single frame
    virtual BindlessHandle AllocateStaticDescriptor(const RHITexture& texture) = 0;
    virtual BindlessHandle AllocateStaticDescriptor(const RHIBuffer& buffer) = 0;
    virtual void FreeStaticDescriptor(BindlessHandle handle) = 0;

    // Dynamic descriptors are only valid while the current frame is in flight
    virtual BindlessHandle AllocateDynamicDescriptor(const RHITexture& texture) = 0;
    virtual BindlessHandle AllocateDynamicDescriptor(const RHIBuffer& buffer) = 0;
    virtual void FreeDynamicDescriptor(BindlessHandle handle) = 0;

    virtual bool IsValid(BindlessHandle handle) = 0;
};

} // namespace vex