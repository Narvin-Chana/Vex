#include "VexTest.h"

#include <gtest/gtest.h>

#include <Vex.h>

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

} // namespace vex