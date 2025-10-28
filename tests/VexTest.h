﻿#pragma once
#include <gtest/gtest.h>

#include <Vex.h>

namespace vex
{
template <class ParamT>
struct VexTestParam : testing::TestWithParam<ParamT>
{
    GfxBackend graphics;

    VexTestParam()
        : graphics{ GraphicsCreateDesc{
              .useSwapChain = false,
              .enableGPUDebugLayer = false,
              .enableGPUBasedValidation = false,
          } }
    {
        GLogger.SetLogLevelFilter(Warning);
    }
};

struct VexTest : testing::Test
{
    GfxBackend graphics;

    VexTest()
        : graphics{ GraphicsCreateDesc{
              .useSwapChain = false,
              .enableGPUDebugLayer = false,
              .enableGPUBasedValidation = false,
          } }
    {
        GLogger.SetLogLevelFilter(Warning);
    }
};

} // namespace vex