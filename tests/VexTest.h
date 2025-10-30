#pragma once
#include <gtest/gtest.h>

#include <Vex.h>

namespace vex
{
static const auto VexRootPath = std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path();

template <class ParamT>
struct VexTestParam : testing::TestWithParam<ParamT>
{
    GfxBackend graphics;

    VexTestParam()
        : graphics{ BackendDescription{
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
    GfxBackend graphics;

    VexTest()
        : graphics{ BackendDescription{
              .useSwapChain = false,
              .enableGPUDebugLayer = false,
              .enableGPUBasedValidation = false,
          } }
    {
        GLogger.SetLogLevelFilter(Warning);
    }
};

} // namespace vex