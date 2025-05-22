#pragma once

#include <Vex/Containers/FreeList.h>
#include <Vex/Handle.h>
#include <Vex/Resource.h>
#include <Vex/RHI/RHIFwd.h>

namespace vex
{

struct BindlessHandle : public Handle<BindlessHandle>
{
};

static constexpr BindlessHandle GInvalidBindlessHandle;

class RHIDescriptorPool
{
public:
    virtual ~RHIDescriptorPool() = default;
    virtual BindlessHandle AllocateStaticDescriptor(const RHITexture& texture) = 0;
    virtual BindlessHandle AllocateStaticDescriptor(const RHIBuffer& buffer) = 0;
    virtual void FreeStaticDescriptor(BindlessHandle handle) = 0;

    virtual BindlessHandle AllocateDynamicDescriptor(const RHITexture& texture) = 0;
    virtual BindlessHandle AllocateDynamicDescriptor(const RHIBuffer& buffer) = 0;
    virtual void FreeDynamicDescriptor(BindlessHandle handle) = 0;

    virtual bool IsValid(BindlessHandle handle) = 0;
};

} // namespace vex