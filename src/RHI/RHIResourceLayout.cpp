#include "RHIResourceLayout.h"

namespace vex
{

RHIResourceLayoutBase::RHIResourceLayoutBase() = default;

void RHIResourceLayoutBase::SetStaticSamplers(Span<const StaticTextureSampler> newSamplers,
                                              std::vector<RHIDeferredRelease>& deferredReleases)
{
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
