#pragma once

#include "RenderDoc.h"

#include <gtest/gtest.h>

#include <Vex.h>

namespace vex
{

static const auto VexRootPath =
    std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path().parent_path();

// Tests are ran in Development, in Debug we should enable GPU validation to ease test development.

struct RenderDocInitializer
{
    RenderDocInitializer()
    {
        RenderDoc::Setup();
    }
};
inline RenderDocInitializer initializer{};

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

    void SetUp() override
    {
        RenderDoc::StartCapture(testing::UnitTest::GetInstance()->current_test_info()->name());
    }

    void TearDown() override
    {
        RenderDoc::EndCapture();
    }
};

// Raytracing-specific version which will skip RT-tests if not supported on the current device.
struct RTVexTest : VexTest
{
    void SetUp() override
    {
        if (!graphics.IsRayTracingSupported())
        {
            GTEST_SKIP() << "Raytracing is not supported, skipping RT-related tests.";
        }

        VexTest::SetUp();
    }
};

const auto ShaderCompilerBackendValues = testing::Values(ShaderCompilerBackend::DXC, ShaderCompilerBackend::Slang);

inline std::string_view GetShaderExtension(ShaderCompilerBackend backend)
{
    switch (backend)
    {
    case ShaderCompilerBackend::DXC:
        return "hlsl";
    case ShaderCompilerBackend::Slang:
        return "slang";
    default:
        VEX_ASSERT(false);
    }
    std::unreachable();
}

struct VexPerShaderCompilerTest : VexTestParam<ShaderCompilerBackend>
{
    static ShaderCompilerBackend GetShaderCompilerBackend()
    {
        return GetParam();
    }
};

template <class T>
struct VexPerShaderCompilerTestParam : VexTestParam<std::tuple<ShaderCompilerBackend, T>>
{
    ShaderCompilerBackend GetShaderCompilerBackend()
    {
        return std::get<0>(VexTestParam<std::tuple<ShaderCompilerBackend, T>>::GetParam());
    }

    T GetParam()
    {
        return std::get<1>(VexTestParam<std::tuple<ShaderCompilerBackend, T>>::GetParam());
    }
};

#define INSTANTIATE_PER_SHADER_COMPILER_TEST_SUITE_P(name, type, values)                                               \
    INSTANTIATE_TEST_SUITE_P(name, type, testing::Combine(ShaderCompilerBackendValues, values));

struct VexPerQueueTest : VexTestParam<QueueType>
{
};

template <class T>
struct VexPerQueueTestWithParam : VexTestParam<std::tuple<QueueType, T>>
{
    QueueType GetQueueType()
    {
        return std::get<0>(VexTestParam<std::tuple<QueueType, T>>::GetParam());
    }

    T GetParam()
    {
        return std::get<1>(VexTestParam<std::tuple<QueueType, T>>::GetParam());
    }
};

const auto QueueTypeValue = testing::Values(QueueType::Graphics, QueueType::Compute, QueueType::Copy);

#define INSTANTIATE_PER_QUEUE_TEST_SUITE_P(name, type, values)                                                         \
    INSTANTIATE_TEST_SUITE_P(name, type, testing::Combine(QueueTypeValue, values));

} // namespace vex