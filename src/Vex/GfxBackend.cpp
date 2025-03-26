#include "GfxBackend.h"
#include <memory>

namespace vex
{
std::unique_ptr<GfxBackend> GGraphics = nullptr;

GfxBackend* CreateGraphicsBackend(const BackendDescription& description)
{
    if (GGraphics)
    {
        // LOG fatal error
        return nullptr;
    }

    GGraphics = std::make_unique<GfxBackend>(description);
    return GGraphics.get();
}

void DestroyGraphicsBackend(GfxBackend* backend)
{
    if (GGraphics.get() != backend)
    {
        // LOG fatal error
        return;
    }

    GGraphics.reset();
}

GfxBackend::GfxBackend(const BackendDescription& description)
    : description(description)
{
}

} // namespace vex
