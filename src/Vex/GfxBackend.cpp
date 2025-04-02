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

    VEX_LOG(Info,
            "Created graphics backend with width {} and height {}.",
            description.platformWindow.width,
            description.platformWindow.height);

    // Option A:
    VEX_LOG(Info, "Feature check: {}", rhi->GetFeatureChecker().IsFeatureSupported(Feature::MeshShader));

    // Option B:
    // featureChecker = rhi->CreateFeatureChecker();
    // featureChecker->IsFeatureSupported(Feature::MeshShader);
}

GfxBackend::~GfxBackend() = default;

} // namespace vex
