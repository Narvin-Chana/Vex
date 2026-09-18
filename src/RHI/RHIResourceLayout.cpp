#include "RHIResourceLayout.h"

#include <bitset>

#include <Vex/Bindings.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHIImpl/RHIPhysicalDevice.h>

namespace vex
{

RHIResourceLayoutBase::RHIResourceLayoutBase(Span<const PipelineStage> stageConstants)
    : supportedStages(stageConstants.begin(), stageConstants.end())
    , maxLocalConstantsByteSize(GPhysicalDevice->GetMaxLocalConstantsByteSize())
{
    for (auto _ : supportedStages)
    {
        stageLocalConstants.emplace_back().resize(maxLocalConstantsByteSize / supportedStages.size());
    }
}

RHIResourceLayoutBase::~RHIResourceLayoutBase() = default;

void RHIResourceLayoutBase::SetLayoutResources(Span<const std::byte> constants, Flags<PipelineStage> stageFlags)
{
    Flags<PipelineStage> supportedStageFlags;
    for (const PipelineStage stage : supportedStages)
        supportedStageFlags |= stage;

    stageFlags &= supportedStageFlags;

    if (!constants.empty() && !stageFlags.IsEmpty())
    {
        const u32 maxByteSizePerStage = maxLocalConstantsByteSize / supportedStages.size();
        if (constants.size_bytes() > maxByteSizePerStage)
        {
            VEX_LOG(Fatal,
                    "Cannot pass in more bytes as local constants versus what your platform allows. You passed in {} "
                    "bytes, your graphics API allows for {} bytes.",
                    constants.size_bytes(),
                    maxByteSizePerStage)
            return;
        }

        for (int i = 0; i < supportedStages.size(); ++i)
        {
            if (const PipelineStage stage = supportedStages[i]; stageFlags.IsSet(stage))
            {
                std::memcpy(stageLocalConstants[i].data(), constants.data(), constants.size_bytes());
            }
        }

        isDirty = true;
    }
}

void RHIResourceLayoutBase::SetStaticSamplers(Span<const StaticTextureSampler> newSamplers)
{
    staticSamplers = { newSamplers.begin(), newSamplers.end() };
    isDirty = true;
}

Span<const StaticTextureSampler> RHIResourceLayoutBase::GetStaticSamplers() const
{
    return staticSamplers;
}

Span<const byte> RHIResourceLayoutBase::GetLocalConstantsData(u32 stageIndex) const
{
    VEX_ASSERT(stageIndex < stageLocalConstants.size());
    return stageLocalConstants[stageIndex];
}

} // namespace vex