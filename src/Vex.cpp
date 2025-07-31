#include "Vex.h"

#include <Vex/Logger.h>
#include <Vex/RHIImpl/RHI.h>

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
        return nullptr;
    }

    return MakeUnique<GfxBackend>(description);
}

} // namespace vex