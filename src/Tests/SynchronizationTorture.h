#pragma once

namespace vex
{

class GfxBackend;

// Used to test out synchronization, contains many edge cases and convoluted dependencies.
void SynchronizationTortureTest(GfxBackend& graphics);

} // namespace vex