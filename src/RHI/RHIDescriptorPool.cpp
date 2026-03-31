#include "RHIDescriptorPool.h"

#include <Vex/Logger.h>

namespace vex
{

RHIDescriptorPoolBase::RHIDescriptorPoolBase()
    : allocator({
          .generations = std::vector<u8>(GDefaultDescriptorPoolSize),
          .handles = FreeListAllocator(GDefaultDescriptorPoolSize),
      })
{
}

BindlessHandle RHIDescriptorPoolBase::AllocateStaticDescriptor(DescriptorType descriptorType)
{
    auto& [generations, handles] = descriptorType == DescriptorType::Resource ? allocator : samplerAllocator;
    if (handles.freeIndices.empty())
    {
        // TODO(https://trello.com/c/uGignSlW): Add resizing, would require copying previous descriptors into the new heap AND making sure the heap
        // survives for atleast FrameBuffering frames. Resize(gpuHeap.size() * 2);
        VEX_LOG(Fatal, "Ran out of static descriptors...");
    }

    const u32 index = handles.Allocate();
    return BindlessHandle::CreateHandle(index, generations[index]);
}

void RHIDescriptorPoolBase::FreeStaticDescriptor(DescriptorType descriptorType, BindlessHandle handle)
{
    auto& [generations, handles] = descriptorType == DescriptorType::Resource ? allocator : samplerAllocator;
    const u32 index = handle.GetIndex();
    generations[index]++;
    handles.Deallocate(index);
    // Clear out the resource from the slot to ensure that the GPU crashes if attempting to access this.
    CopyNullDescriptor(descriptorType, index);
}

bool RHIDescriptorPoolBase::IsValid(BindlessHandle handle)
{
    return handle.GetGeneration() == allocator.generations[handle.GetIndex()];
}
} // namespace vex