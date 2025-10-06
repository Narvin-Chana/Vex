#pragma once
#include <gtest/gtest.h>

#include <Vex.h>

namespace vex
{
template <class ParamT>
struct VexTestParam : testing::TestWithParam<ParamT>
{
    GfxBackend graphics;

    VexTestParam()
        : graphics{ BackendDescription{
              .useSwapChain = false,
              .enableGPUDebugLayer = false,
              .enableGPUBasedValidation = false,
          } }
    {
    }

    ~VexTestParam() override
    {
        graphics.FlushGPU();
    }
};

struct VexTest : testing::Test
{
    GfxBackend graphics;

    VexTest()
        : graphics{ BackendDescription{
              .useSwapChain = false,
              .enableGPUDebugLayer = false,
              .enableGPUBasedValidation = false,
          } }
    {
    }

    ~VexTest() override
    {
        graphics.FlushGPU();
    }
};

} // namespace vex