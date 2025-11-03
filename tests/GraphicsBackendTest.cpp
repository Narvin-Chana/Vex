#include "VexTest.h"

#include <gtest/gtest.h>

#include <Vex.h>

namespace vex
{

TEST(GraphicsTests, CreateGraphicsWithoutDebugLayers)
{
    Graphics{ BackendDescription{
        .useSwapChain = false,
        .enableGPUDebugLayer = false,
        .enableGPUBasedValidation = false,
    } };
}

TEST(GraphicsTests, CreateGraphicsDebugLayersAndGPUValidation)
{
    Graphics{ BackendDescription{
        .useSwapChain = false,
        .enableGPUDebugLayer = true,
        .enableGPUBasedValidation = true,
    } };
}

TEST(GraphicsTests, CreateGraphicsDebugLayerWithoutGPUValidation)
{
    Graphics{ BackendDescription{
        .useSwapChain = false,
        .enableGPUDebugLayer = true,
        .enableGPUBasedValidation = false,
    } };
}

} // namespace vex