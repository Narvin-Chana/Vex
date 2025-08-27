#include "RHIResourceLayout.h"

#include <utility>

#include <Vex/Bindings.h>
#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHIBindings.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHITexture.h>

namespace vex
{

RHIResourceLayoutBase::RHIResourceLayoutBase()
    : maxLocalConstantsByteSize(GPhysicalDevice->featureChecker->GetMaxLocalConstantsByteSize())
{
    localConstantsData.reserve(maxLocalConstantsByteSize);
}

RHIResourceLayoutBase::~RHIResourceLayoutBase() = default;

void RHIResourceLayoutBase::SetLayoutResources(RHI& rhi,
                                               ResourceCleanup& resourceCleanup,
                                               const std::optional<ConstantBinding>& constants,
                                               std::span<RHITextureBinding> textures,
                                               std::span<RHIBufferBinding> buffers,
                                               RHIDescriptorPool& descriptorPool,
                                               RHIAllocator& allocator)
{
    if (constants.has_value())
    {
        localConstantsData.resize(constants->size);
        std::memcpy(localConstantsData.data(), constants->data, constants->size);
    }

    // Remove and mark for deletion the previous buffer (if any)
    if (currentInternalConstantBuffer)
    {
        resourceCleanup.CleanupResource(std::move(currentInternalConstantBuffer));
        currentInternalConstantBuffer = std::nullopt;
    }

    std::vector<std::pair<std::string, BindlessHandle>> bindlessNamesAndHandles;
    // Allocate for worst-case scenario.
    bindlessNamesAndHandles.reserve(textures.size() + buffers.size());
    for (auto& [binding, texture] : textures)
    {
        bindlessNamesAndHandles.emplace_back(binding.name, texture->GetOrCreateBindlessView(binding, descriptorPool));
    }

    for (auto& [binding, buffer] : buffers)
    {
        bindlessNamesAndHandles.emplace_back(
            binding.name,
            buffer->GetOrCreateBindlessView(binding.usage, binding.stride, descriptorPool));
    }

    // Accumulate then sort, to have a deterministic order.
    std::sort(bindlessNamesAndHandles.begin(),
              bindlessNamesAndHandles.end(),
              [](const std::pair<std::string, BindlessHandle>& lh, const std::pair<std::string, BindlessHandle>& rh)
              { return lh.first < rh.first; });

    std::vector<u32> bindlessHandleIndexData;
    bindlessHandleIndexData.reserve(bindlessNamesAndHandles.size());
    for (auto& [name, handle] : bindlessNamesAndHandles)
    {
        bindlessHandleIndexData.emplace_back(handle.GetIndex());
    }

    // Tmp buffer for uploading the bindless constants.. this doesn't scale well...
    // Ideally we'd allocate a BAB (big-ass-buffer) and then use subsections in it (or even better, have an
    // allocation strategy that allocates huge chunks of memory and create as many buffers as we want in it).
    // Instead we just greedily allocate buffers for now that are automatically destroyed after use.
    if (!bindlessNamesAndHandles.empty())
    {
        currentInternalConstantBuffer = rhi.CreateBuffer(
            allocator,
            BufferDescription{ .name = "BindlessIdxBuffer",
                               .byteSize = static_cast<u32>(bindlessHandleIndexData.size() * sizeof(u32)),
                               .usage = BufferUsage::UniformBuffer,
                               .memoryLocality = ResourceMemoryLocality::CPUWrite });
        ResourceMappedMemory(*currentInternalConstantBuffer)
            .SetData(std::span{ reinterpret_cast<const u8*>(bindlessHandleIndexData.data()),
                                bindlessHandleIndexData.size() * sizeof(u32) });
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