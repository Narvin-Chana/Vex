#include "ResourceCleanup.h"

#include <Vex/Debug.h>
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

void ResourceCleanup::FlushResources(i8 flushCount, RHIDescriptorPool& descriptorPool)
{
    std::vector<CleanupVariant> resourcesToDestroy;
    for (auto& [resource, lifespan] : resourcesInFlight)
    {
        lifespan = std::max(lifespan - flushCount, 0);
    }

    for (auto& [resource, lifespan] : resourcesInFlight)
    {
        if (lifespan == 0)
        {
            std::visit(
                [&descriptorPool](auto& val)
                {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<UniqueHandle<RHITexture>, T>)
                    {
                        val->FreeBindlessHandles(descriptorPool);
                    }
                    else if constexpr (std::is_same_v<UniqueHandle<RHIBuffer>, T>)
                    {
                        VEX_NOT_YET_IMPLEMENTED();
                    }
                    else
                    {
                        // Nothing to do.
                    }
                },
                resource);
        }
    }

    std::erase_if(resourcesInFlight, [](const std::pair<CleanupVariant, i8>& val) { return val.second == 0; });
}

} // namespace vex
