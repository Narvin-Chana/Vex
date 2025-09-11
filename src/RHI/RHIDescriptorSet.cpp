#include "RHIDescriptorSet.h"

#include "Vex/Logger.h"

namespace vex
{

RHIBindlessDescriptorSetBase::RHIBindlessDescriptorSetBase()
    : allocator({
          .generations = std::vector<u8>(GDefaultDescriptorPoolSize),
          .handles = FreeListAllocator(GDefaultDescriptorPoolSize),
      })
{
}

BindlessHandle RHIBindlessDescriptorSetBase::AllocateStaticDescriptor()
{
    if (allocator.handles.freeIndices.empty())
    {
        // TODO: Add resizing, would require copying previous descriptors into the new heap AND making sure the heap
        // survives for atleast FrameBuffering frames. Resize(gpuHeap.size() * 2);
        VEX_LOG(Fatal, "Ran out of static descriptors...");
    }

    u32 index = allocator.handles.Allocate();
    return BindlessHandle::CreateHandle(index, allocator.generations[index]);
}

void RHIBindlessDescriptorSetBase::FreeStaticDescriptor(BindlessHandle handle)
{
    const u32 index = handle.GetIndex();
    auto& [generations, handles] = allocator;
    generations[index]++;
    handles.Deallocate(index);
    // Clear out the resource from the slot to ensure that the GPU crashes if attempting to access this.
    CopyNullDescriptor(index);
}

BindlessHandle RHIBindlessDescriptorSetBase::AllocateDynamicDescriptor()
{
    VEX_NOT_YET_IMPLEMENTED();
    return BindlessHandle();
}

void RHIBindlessDescriptorSetBase::FreeDynamicDescriptor(BindlessHandle handle)
{
    VEX_NOT_YET_IMPLEMENTED();
}

bool RHIBindlessDescriptorSetBase::IsValid(BindlessHandle handle)
{
    return handle.GetGeneration() == allocator.generations[handle.GetIndex()];
}
} // namespace vex