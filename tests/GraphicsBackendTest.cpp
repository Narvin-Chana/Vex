#include "VexTest.h"

namespace vex
{

TEST(GraphicsTests, CreateGraphicsWithoutDebugLayers)
{
    Graphics{ GraphicsCreateDesc{
        .useSwapChain = false,
        .enableGPUDebugLayer = false,
        .enableGPUBasedValidation = false,
    } };
}

TEST(GraphicsTests, CreateGraphicsDebugLayersAndGPUValidation)
{
    Graphics{ GraphicsCreateDesc{
        .useSwapChain = false,
        .enableGPUDebugLayer = true,
        .enableGPUBasedValidation = true,
    } };
}

TEST(GraphicsTests, CreateGraphicsDebugLayerWithoutGPUValidation)
{
    Graphics{ GraphicsCreateDesc{
        .useSwapChain = false,
        .enableGPUDebugLayer = true,
        .enableGPUBasedValidation = false,
    } };
}

TEST(GraphicsTests, PhysicalDeviceSelection_SecondDevice)
{
    std::vector<RHIPhysicalDevice*> devices = Graphics::EnumeratePhysicalDevices();

    Graphics{ GraphicsCreateDesc{ .useSwapChain = false,
                                  .enableGPUDebugLayer = true,
                                  .enableGPUBasedValidation = false,
                                  .specifiedDevice = devices.back() } };

    Graphics{ GraphicsCreateDesc{ .useSwapChain = false,
                                  .enableGPUDebugLayer = true,
                                  .enableGPUBasedValidation = false,
                                  .specifiedDevice = devices.front() } };
}

} // namespace vex