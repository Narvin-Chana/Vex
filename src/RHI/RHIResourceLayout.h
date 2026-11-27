#pragma once

#include <vector>

#include <Vex/Containers/Span.h>
#include <Vex/TextureSampler.h>
#include <Vex/Types.h>

#include <RHI/RHIFwd.h>

namespace vex
{

class RHIResourceLayoutBase
{
public:
    RHIResourceLayoutBase();
    RHIResourceLayoutBase(RHIResourceLayoutBase&&) = default;
    RHIResourceLayoutBase& operator=(RHIResourceLayoutBase&&) = default;

    void SetStaticSamplers(Span<const StaticTextureSampler> newSamplers,
                           std::vector<RHIDeferredRelease>& deferredReleases);
    Span<const StaticTextureSampler> GetStaticSamplers() const;

    u32 version = 0;

protected:
    virtual void Cleanup(std::vector<RHIDeferredRelease>& deferredReleases) = 0;
    virtual void Compile() = 0;

    void Rebuild();

    std::vector<StaticTextureSampler> staticSamplers;
};

} // namespace vex
