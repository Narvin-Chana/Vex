#include "GfxBackend.h"

#include <Vex/Logger.h>

namespace vex
{
UniqueHandle<GfxBackend> CreateGraphicsBackend(const BackendDescription& description)
{
    if (description.platformWindow.width == 0 || description.platformWindow.height == 0)
    {
        VEX_LOG(Fatal,
                "Unable to create a window with width {} and height {}.",
                description.platformWindow.width,
                description.platformWindow.height);
        return {};
    }

    return MakeUnique<GfxBackend>(description);
}

GfxBackend::GfxBackend(const BackendDescription& description)
    : description(description)
{
    VEX_LOG(Info,
            "Created graphics backend with width {} and height {}.",
            description.platformWindow.width,
            description.platformWindow.height);
}

} // namespace vex
