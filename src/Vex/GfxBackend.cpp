#include "GfxBackend.h"

#include <Vex/FeatureChecker.h>
#include <Vex/Logger.h>
#include <Vex/RHI.h>

namespace vex
{

GfxBackend::GfxBackend(UniqueHandle<RenderHardwareInterface>&& newRHI, const BackendDescription& description)
    : rhi(std::move(newRHI))
    , description(description)
{
    rhi->InitWindow(description.platformWindow);

#if !VEX_SHIPPING
    rhi->GetFeatureChecker().DumpFeatureSupport();
#endif

    VEX_LOG(Info,
            "Created graphics backend with width {} and height {}.",
            description.platformWindow.width,
            description.platformWindow.height);
}

GfxBackend::~GfxBackend() = default;

} // namespace vex
