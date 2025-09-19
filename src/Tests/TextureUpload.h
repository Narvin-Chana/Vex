#pragma once

#include <Vex/CommandQueueType.h>
#include <Vex/NonNullPtr.h>

namespace vex
{

class GfxBackend;

void TestTextureUpload(NonNullPtr<GfxBackend> backend, CommandQueueType type);

} // namespace vex