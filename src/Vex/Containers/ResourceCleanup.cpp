#include "ResourceCleanup.h"

#include <Vex/Debug.h>
#include <Vex/RHIImpl/RHIPipelineState.h>

namespace vex
{

// All underlying types of the ResourceCleanup must be move assignable.
static_assert(std::is_move_assignable_v<ResourceCleanup::CleanupVariant>);

ResourceCleanup::ResourceCleanup(i8 bufferingCount)
    : defaultLifespan(bufferingCount)
{
}

void ResourceCleanup::CleanupResource(CleanupVariant&& resource)
{
    resourcesInFlight.emplace_back(std::move(resource), defaultLifespan);
}

void ResourceCleanup::CleanupResource(CleanupVariant&& resource, i8 lifespan)
{
    resourcesInFlight.emplace_back(std::move(resource), lifespan);
}

void ResourceCleanup::FlushResources(i8 flushCount, RHIDescriptorPool& descriptorPool, RHIAllocator& allocator)
{
    for (size_t i = 0; i < resourcesInFlight.size(); ++i)
    {
        auto& [resource, lifespan] = resourcesInFlight[i];
        lifespan = std::max(lifespan - flushCount, 0);
        if (lifespan <= 0)
        {
            std::visit(
                [&descriptorPool, &allocator](auto& val)
                {
                    using T = std::remove_cvref_t<decltype(val)>;
                    if constexpr (std::is_same_v<MaybeUninitialized<RHITexture>, T> or
                                  std::is_same_v<MaybeUninitialized<RHIBuffer>, T>)
                    {
                        val->FreeBindlessHandles(descriptorPool);
                        val->FreeAllocation(allocator);
                    }
                    // Reset in any case, works with both UniqueHandle and MaybeUninitialized.
                    val.reset();
                },
                resource);
        }
    }

    std::erase_if(resourcesInFlight,
                  [](const std::pair<CleanupVariant, i8>& elem) -> bool { return elem.second <= 0; });
}

} // namespace vex
