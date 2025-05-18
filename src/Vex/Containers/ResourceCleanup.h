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
    using TCleanupVariant = std::variant<UniqueHandle<RHITexture>, UniqueHandle<RHIBuffer>>;

    void CleanupResource(TCleanupVariant resource, i8 lifespan);
    void FlushResources(i8 flushCount);

private:
    std::vector<std::pair<TCleanupVariant, i8>> resourcesInFlight;
};

} // namespace vex