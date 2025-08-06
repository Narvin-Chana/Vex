#include "RHIResourceLayout.h"

#include <Vex/Bindings.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHIBindings.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>

namespace vex
{

RHIResourceLayoutBase::RHIResourceLayoutBase()
    : maxLocalConstantsByteSize(GPhysicalDevice->featureChecker->GetMaxLocalConstantsByteSize())
    , localConstantsData(maxLocalConstantsByteSize)
    // TEMP
    , internalConstantBuffers(static_cast<FrameBuffering>(6))
{
}

void RHIResourceLayoutBase::SetLayoutResources(std::span<const ConstantBinding> constants,
                                               std::span<RHITextureBinding> textures,
                                               std::span<RHIBufferBinding> buffers,
                                               RHIDescriptorPool& descriptorPool)
{
    FrameIndex = (FrameIndex + 1) % 6;

    localConstantsData.resize(maxLocalConstantsByteSize);
#if !VEX_SHIPPING
    // Fill the buffer with FF to mark as garbage.
    for (u32 i = 0; i < localConstantsData.size(); ++i)
    {
        localConstantsData[i] = 0xFF;
    }
#endif

    u32 currentlyUsedBytes =
        ConstantBinding::ConcatConstantBindings(constants, { localConstantsData.begin(), localConstantsData.end() });
    localConstantsData.resize(currentlyUsedBytes);

    std::vector<u32> bindlessHandles;
    // Allocate for worst-case scenario.
    bindlessHandles.reserve(textures.size() + buffers.size());
    for (auto& [binding, usage, texture] : textures)
    {
        if (usage & TextureUsage::ShaderRead || usage & TextureUsage::ShaderReadWrite)
        {
            bindlessHandles.push_back(texture->GetOrCreateBindlessView(binding, usage, descriptorPool).GetIndex());
        }
    }

    for (auto& [binding, buffer] : buffers)
    {
        bindlessHandles.push_back(
            buffer->GetOrCreateBindlessView(binding.bufferUsage, binding.bufferStride, descriptorPool).GetIndex());
    }

    // Tmp buffer for uploading the bindless constants.. this doesn't scale for more than one type of pass per frame...
    // Ideally we'd allocate a BAB (big-ass-buffer) and then use subsections in it (or even better, have an allocation
    // strategy that allocates huge chunks of memory and create as many buffers as we want in it). Instead we just
    // oscillate between 3 hardcoded buffers for now.
    if (!bindlessHandles.empty())
    {
        internalConstantBuffers.Get(FrameIndex)
            ->GetMappedMemory()
            .SetData(
                std::span<u8>{ reinterpret_cast<u8*>(bindlessHandles.data()), bindlessHandles.size() * sizeof(u32) });
    }
}

void RHIResourceLayoutBase::SetSamplers(std::span<TextureSampler> newSamplers)
{
    // Validation of the sampler name.
    for (auto& s : newSamplers)
    {
        if (s.name.empty())
        {
            VEX_LOG(Fatal,
                    "Your sampler state must have a valid name, otherwise you cannot use the sampler in shaders!");
        }
    }

    samplers = { newSamplers.begin(), newSamplers.end() };
    isDirty = true;
}

std::span<const TextureSampler> RHIResourceLayoutBase::GetStaticSamplers() const
{
    return samplers;
}

std::span<const u8> RHIResourceLayoutBase::GetLocalConstantsData() const
{
    return localConstantsData;
}

} // namespace vex