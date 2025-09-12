#pragma once

#include <Vex/NonNullPtr.h>

namespace vex
{

class GfxBackend;

void TestTextureUpload(NonNullPtr<GfxBackend> backend);

} // namespace vex