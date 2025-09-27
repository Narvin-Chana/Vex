#include "Vex.h"

#include <Vex/Logger.h>
#include <Vex/RHIImpl/RHI.h>

namespace vex
{

UniqueHandle<GfxBackend> CreateGraphicsBackend(const GraphicsCreateDesc& desc)
{
    return MakeUnique<GfxBackend>(desc);
}

} // namespace vex