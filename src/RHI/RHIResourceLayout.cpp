#include "RHIResourceLayout.h"

#include <utility>

#include <Vex/Bindings.h>
#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Validation.h>

#include <RHI/RHIBindings.h>

namespace vex
{

RHIResourceLayoutBase::RHIResourceLayoutBase()
    : maxLocalConstantsByteSize(GPhysicalDevice->featureChecker->GetMaxLocalConstantsByteSize())
{
    localConstantsData.reserve(maxLocalConstantsByteSize);
}

RHIResourceLayoutBase::~RHIResourceLayoutBase() = default;

void RHIResourceLayoutBase::SetConstants(ConstantBinding constants)
{

    localConstantsData.resize(constants.data.size_bytes());
    std::memcpy(localConstantsData.data(), constants.data.data(), constants.data.size_bytes());
}

void RHIResourceLayoutBase::SetSamplers(std::span<TextureSampler> newSamplers)
{
    samplers = { newSamplers.begin(), newSamplers.end() };
    isDirty = true;
}

std::span<const TextureSampler> RHIResourceLayoutBase::GetStaticSamplers() const
{
    return samplers;
}

} // namespace vex