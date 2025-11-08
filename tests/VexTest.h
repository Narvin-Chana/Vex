#pragma once
#include <gtest/gtest.h>

#include <Vex.h>

namespace vex
{
static const auto VexRootPath =
    std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path().parent_path();

// Tests are ran in Development, in Debug we should enable GPU validation to ease test development.

template <class ParamT>
struct VexTestParam : testing::TestWithParam<ParamT>
{
    Graphics graphics;

    VexTestParam()
        : graphics{ GraphicsCreateDesc{
              .useSwapChain = false,
              .enableGPUDebugLayer = VEX_DEBUG,
              .enableGPUBasedValidation = VEX_DEBUG,
              .shaderCompilerSettings = { .shaderIncludeDirectories = { VexRootPath / "shaders" } } } }
    {
        GLogger.SetLogLevelFilter(Warning);
    }
};

struct VexTest : testing::Test
{
    Graphics graphics;

    VexTest()
        : graphics{ GraphicsCreateDesc{
              .useSwapChain = false,
              .enableGPUDebugLayer = VEX_DEBUG,
              .enableGPUBasedValidation = VEX_DEBUG,
          } }
    {
        GLogger.SetLogLevelFilter(Warning);
    }
};

} // namespace vex