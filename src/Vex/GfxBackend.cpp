#include "GfxBackend.h"

#include <Vex/Logger.h>
#include <Vex/RHI.h>

namespace vex
{

GfxBackend::GfxBackend(UniqueHandle<RenderHardwareInterface>&& rhi, const BackendDescription& description)
    : rhi(std::move(rhi))
    , description(description)
{
    VEX_LOG(Info,
            "Created graphics backend with width {} and height {}.",
            description.platformWindow.width,
            description.platformWindow.height);
}

GfxBackend::~GfxBackend() = default;

} // namespace vex
