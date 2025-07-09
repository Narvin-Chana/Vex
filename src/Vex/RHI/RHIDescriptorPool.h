#pragma once

#include <Vex/Containers/FreeList.h>
#include <Vex/Handle.h>
#include <Vex/RHI/RHIFwd.h>
#include <Vex/Resource.h>

namespace vex
{

struct BindlessHandle : public Handle<BindlessHandle>
{
    enum class Type : i8
    {
        None = -1,
        UniformBuffer = 0,
        StorageBuffer = 1,
        Texture = 2,
        StorageImage = 3,
        Num = 4
    } type = Type::None;

    static BindlessHandle CreateHandle(u32 index, u8 generation, Type type)
    {
        BindlessHandle handle = Handle::CreateHandle(index, generation);
        handle.type = type;
        return handle;
    }
};

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