#pragma once
#include <gtest/gtest.h>

#include <Vex.h>

namespace vex
{
static const auto VexRootPath =
    std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path().parent_path();

template <class ParamT>
struct VexTestParam : testing::TestWithParam<ParamT>
{
    Graphics graphics;

    VexTestParam()
        : graphics{ GraphicsCreateDesc{
              .useSwapChain = false,
              .enableGPUDebugLayer = false,
              .enableGPUBasedValidation = false,
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
              .enableGPUDebugLayer = false,
              .enableGPUBasedValidation = false,
          } }
    {
        GLogger.SetLogLevelFilter(Warning);
    }
};

} // namespace vex