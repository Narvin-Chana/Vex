#pragma once

#include <variant>
#include <vector>

#include <Vex/RHI/RHIBuffer.h>
#include <Vex/RHI/RHITexture.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

class ResourceCleanup
{
public:
    using CleanupVariant = std::variant<UniqueHandle<RHITexture>, UniqueHandle<RHIBuffer>>;

    void CleanupResource(CleanupVariant resource, i8 lifespan);
    void FlushResources(i8 flushCount);

private:
    std::vector<std::pair<CleanupVariant, i8>> resourcesInFlight;
};

} // namespace vex