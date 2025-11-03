#include "ResourceCleanup.h"

#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHIPipelineState.h>

namespace vex
{

// All underlying types of the ResourceCleanup must be move assignable.
static_assert(std::is_move_assignable_v<ResourceCleanup::CleanupVariant>);

ResourceCleanup::ResourceCleanup(NonNullPtr<RHI> rhi)
    : rhi(rhi)
{
}

void ResourceCleanup::CleanupResource(CleanupVariant&& resource)
{
    resourcesInFlight.emplace_back(std::move(resource), rhi->GetMostRecentSyncTokenPerQueue());
}

void ResourceCleanup::FlushResources(RHIDescriptorPool& descriptorPool, RHIAllocator& allocator)
{
    std::erase_if(resourcesInFlight,
                  [&rhi = rhi, &descriptorPool, &allocator](
                      std::pair<CleanupVariant, std::array<SyncToken, QueueTypes::Count>>& elem) -> bool
                  {
                      // See if all tokens are finished.
                      for (SyncToken& token : elem.second)
                      {
                          if (!rhi->IsTokenComplete(token))
                          {
                              return false;
                          }
                      }

                      // If we reach here, we can now free the resource.
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
                          elem.first);

                      return true;
                  });
}

} // namespace vex
