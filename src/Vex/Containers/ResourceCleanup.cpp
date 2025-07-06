#include "ResourceCleanup.h"

#include <Vex/RHI/RHIBuffer.h>
#include <Vex/RHI/RHIPipelineState.h>
#include <Vex/RHI/RHITexture.h>

namespace vex
{

ResourceCleanup::ResourceCleanup(i8 bufferingCount)
    : defaultLifespan(bufferingCount)
{
}

void ResourceCleanup::CleanupResource(CleanupVariant resource)
{
    resourcesInFlight.emplace_back(std::move(resource), defaultLifespan);
}

void ResourceCleanup::CleanupResource(CleanupVariant resource, i8 lifespan)
{
    resourcesInFlight.emplace_back(std::move(resource), lifespan);
}

void ResourceCleanup::FlushResources(i8 flushCount)
{
    for (auto& [resource, lifespan] : resourcesInFlight)
    {
        lifespan = std::max(lifespan - flushCount, 0);
    }

    std::erase_if(resourcesInFlight, [](const std::pair<CleanupVariant, i8>& val) { return val.second == 0; });
}

} // namespace vex
