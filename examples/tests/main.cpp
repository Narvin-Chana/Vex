#include <Tests/SynchronizationTorture.h>
#include <Tests/TextureUploadTests.h>

#include <Vex/GfxBackend.h>

int main()
{
    vex::GfxBackend backend = vex::GfxBackend(vex::BackendDescription{ .useSwapChain = false,
                                                                       .enableGPUDebugLayer = !VEX_SHIPPING,
                                                                       .enableGPUBasedValidation = !VEX_SHIPPING });

    // TODO: Add start and end capture once we have render doc natively setup for better analysis of results
    // See https://trello.com/c/eGi09y1Y
    vex::TextureUploadDownladTests(backend);
    vex::SynchronizationTortureTest(backend);
}