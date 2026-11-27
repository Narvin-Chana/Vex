#include "RHIResourceLayout.h"

#include <Vex/Logger.h>
#include <VexMacros.h>

namespace vex
{

RHIResourceLayoutBase::RHIResourceLayoutBase() = default;

void RHIResourceLayoutBase::SetStaticSamplers(Span<const StaticTextureSampler> newSamplers,
                                              std::vector<RHIDeferredRelease>& deferredReleases)
{
    VEX_CHECK(newSamplers.size() <= GMaxStaticSamplerCount,
              "Cannot set {} static samplers, the maximum is {}.",
              newSamplers.size(),
              GMaxStaticSamplerCount);

    Cleanup(deferredReleases);
    staticSamplers = { newSamplers.begin(), newSamplers.end() };
    Rebuild();
}

Span<const StaticTextureSampler> RHIResourceLayoutBase::GetStaticSamplers() const
{
    return staticSamplers;
}

void RHIResourceLayoutBase::Rebuild()
{
    Compile();
    ++version;
}

} // namespace vex
