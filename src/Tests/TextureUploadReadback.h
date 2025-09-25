#pragma once

namespace vex
{

class GfxBackend;

// Used to test out synchronization, contains many edge cases and convoluted dependencies.
void TextureUploadDownladTests(GfxBackend& graphics);

} // namespace vex