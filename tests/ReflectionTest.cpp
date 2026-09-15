#include "VexTest.h"

namespace vex
{

struct ComputeShaderReflectionTestParam
{
    std::string_view entryPoint;
    ShaderReflection expectedReflection;
};
struct ComputeShaderReflectionTest : VexPerShaderCompilerTestParam<ComputeShaderReflectionTestParam>
{
};

TEST_P(ComputeShaderReflectionTest, ComputeShaderReflection)
{
    ShaderCompiler compiler{};

    ComputeShaderReflectionTestParam param = GetParam();

    ShaderKey shaderKey{
        .filepath = std::format("{}/tests/shaders/reflection/Semantics.{}",
                                VexRootPath.string(),
                                GetShaderExtension(GetShaderCompilerBackend())),
        .entryPoint = std::string(param.entryPoint),
        .type = ShaderType::ComputeShader,
    };

    auto err = compiler.CompileShaderFromFilepath(shaderKey);
    VEX_ASSERT(!err.has_value());

    auto& shader = *compiler.GetShader(shaderKey);
    ASSERT_TRUE(shader.GetReflection() && (*shader.GetReflection() == param.expectedReflection));
}

INSTANTIATE_PER_SHADER_COMPILER_TEST_SUITE_P(
    PerShaderCompiler,
    ComputeShaderReflectionTest,
    testing::Values(ComputeShaderReflectionTestParam{ "ReflectionCompute1", {} },
                    ComputeShaderReflectionTestParam{ "ReflectionCompute2", {} },
                    ComputeShaderReflectionTestParam{ "ReflectionCompute3", {} }));

struct PixelShaderReflectionTestParam
{
    std::string_view entryPoint;
    ShaderReflection expectedReflection;
};
struct PixelShaderReflectionTest : VexPerShaderCompilerTestParam<PixelShaderReflectionTestParam>
{
};

TEST_P(PixelShaderReflectionTest, PixelShaderReflection)
{
    ShaderCompiler compiler{};

    PixelShaderReflectionTestParam param = GetParam();

    ShaderKey shaderKey{
        .filepath = std::format("{}/tests/shaders/reflection/Semantics.{}",
                                VexRootPath.string(),
                                GetShaderExtension(GetShaderCompilerBackend())),
        .entryPoint = std::string(param.entryPoint),
        .type = ShaderType::PixelShader,
    };

    auto err = compiler.CompileShaderFromFilepath(shaderKey);
    VEX_ASSERT(!err.has_value());

    auto& shader = *compiler.GetShader(shaderKey);
    ASSERT_TRUE(shader.GetReflection() && (*shader.GetReflection() == param.expectedReflection));
}

INSTANTIATE_PER_SHADER_COMPILER_TEST_SUITE_P(PerShaderCompiler,
                                             PixelShaderReflectionTest,
                                             testing::Values(PixelShaderReflectionTestParam{ "ReflectionPixel1", {} }));

} // namespace vex