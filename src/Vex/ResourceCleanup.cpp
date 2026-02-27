#include "ResourceCleanup.h"

#include <type_traits>

namespace vex
{

// All underlying types of the ResourceCleanup must be move assignable.
static_assert(std::is_move_assignable_v<CleanupVariant>);

void CleanupResource(CleanupVariant&& resource, RHIDescriptorPool& descriptorPool, RHIAllocator& allocator)
{
    std::visit(
        [&descriptorPool, &allocator](auto& val)
        {
            using T = std::remove_cvref_t<decltype(val)>;
            if constexpr (std::is_same_v<std::unique_ptr<RHITexture>, T> or
                          std::is_same_v<std::unique_ptr<RHIBuffer>, T> or
                          std::is_same_v<MaybeUninitialized<RHIBuffer>, T> or
                          std::is_same_v<std::unique_ptr<RHIAccelerationStructure>, T>)
            {
                val->FreeBindlessHandles(descriptorPool);
                val->FreeAllocation(allocator);
            }
            // Reset to free the resource, works with both std::unique_ptr and MaybeUninitialized.
            val.reset();
        },
        resource);
}

} // namespace vex
