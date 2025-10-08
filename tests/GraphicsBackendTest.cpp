#include "VexTest.h"

#include <gtest/gtest.h>

#include <Vex.h>

namespace vex
{

TEST(GraphicsTests, CreateGraphicsWithoutDebugLayers)
{
    GfxBackend{ BackendDescription{
        .useSwapChain = false,
        .enableGPUDebugLayer = false,
        .enableGPUBasedValidation = false,
    } };
}

TEST(GraphicsTests, CreateGraphicsDebugLayersAndGPUValidation)
{
    GfxBackend{ BackendDescription{
        .useSwapChain = false,
        .enableGPUDebugLayer = true,
        .enableGPUBasedValidation = true,
    } };
}

TEST(GraphicsTests, CreateGraphicsDebugLayerWithoutGPUValidation)
{
    GfxBackend{ BackendDescription{
        .useSwapChain = false,
        .enableGPUDebugLayer = true,
        .enableGPUBasedValidation = false,
    } };
}

} // namespace vex