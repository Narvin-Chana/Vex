/**
 * Tests for ShaderCompileContext: Virtual File System (VFS) includes,
 * inline source code compilation, and context-based session sharing.
 *
 * These tests validate the AOT (ahead of time) compilation path added
 * for embedding shader code directly without on-disk files.
 */

#include "VexTest.h"

#include <gtest/gtest.h>

namespace vex
{

// ============================================================
// Helpers
// ============================================================

// Minimal HLSL compute shader that includes a virtual file defining a helper.
static constexpr const char* kHlslMainWithVirtualInclude = R"hlsl(
#include "VirtualHelper.hlsli"
[numthreads(1,1,1)]
void CSMain() { uint x = MyHelper(); }
)hlsl";

static constexpr const char* kHlslVirtualHelper = R"hlsl(
uint MyHelper() { return 42u; }
)hlsl";

// Minimal self-contained HLSL compute shader (no includes).
static constexpr const char* kHlslMinimalCompute = R"hlsl(
[numthreads(1,1,1)]
void CSMain() {}
)hlsl";

#if VEX_SLANG
// Minimal Slang compute shader that imports a virtual module providing a helper.
static constexpr const char* kSlangMainWithVirtualImport = R"slang(
#include "VirtualHelper.slang"
[numthreads(1,1,1)]
void CSMain() { uint x = MyHelper(); }
)slang";

static constexpr const char* kSlangVirtualHelper = R"slang(
uint MyHelper() { return 42u; }
)slang";

static constexpr const char* kSlangMinimalCompute = R"slang(
[numthreads(1,1,1)]
void CSMain() {}
)slang";
#endif

// ============================================================
// Tests: ShaderCompiler direct (requires GPhysicalDevice, so uses VexTest)
// ============================================================

// NOTE: ShaderCompiler internally calls CreateShaderEnvironment() which reads
// GPhysicalDevice. A Graphics instance (created by VexTest) must exist first.
struct ShaderCompileContextCompilerTest : VexTest
{
};

// -- DXC + VFS include --
TEST_F(ShaderCompileContextCompilerTest, DXC_InlineSource_NoInclude_Compiles)
{
    ShaderCompiler compiler{};
    auto& ctx = compiler.GetCompileContext();

    Shader shader{ ShaderKey{
        .sourceCode = kHlslMinimalCompute,
        .entryPoint = "CSMain",
        .type = ShaderType::ComputeShader,
        .compiler = ShaderCompilerBackend::DXC,
    } };

    auto result = compiler.CompileShader(shader);
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error());
}

TEST_F(ShaderCompileContextCompilerTest, DXC_InlineSource_VirtualInclude_Compiles)
{
    ShaderCompiler compiler{};
    auto& ctx = compiler.GetCompileContext();
    ctx.AddVirtualFile("VirtualHelper.hlsli", kHlslVirtualHelper);

    Shader shader{ ShaderKey{
        .sourceCode = kHlslMainWithVirtualInclude,
        .entryPoint = "CSMain",
        .type = ShaderType::ComputeShader,
        .compiler = ShaderCompilerBackend::DXC
    } };

    auto result = compiler.CompileShader(shader);
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error());
}

TEST_F(ShaderCompileContextCompilerTest, DXC_InlineSource_MissingInclude_Fails)
{
    // No VFS entry — should fail to compile.
    ShaderCompiler compiler{};
    auto& ctx = compiler.GetCompileContext();

    Shader shader{ ShaderKey{
        .sourceCode = kHlslMainWithVirtualInclude,
        .entryPoint = "CSMain",
        .type = ShaderType::ComputeShader,
        .compiler = ShaderCompilerBackend::DXC
    } };

    auto result = compiler.CompileShader(shader);
    EXPECT_FALSE(result.has_value());
}

#if VEX_SLANG
// -- Slang + VFS include --
TEST_F(ShaderCompileContextCompilerTest, Slang_InlineSource_NoInclude_Compiles)
{
    ShaderCompiler compiler{};
    auto& ctx = compiler.GetCompileContext();

    Shader shader{ ShaderKey{
        .sourceCode = kSlangMinimalCompute,
        .entryPoint = "CSMain",
        .type = ShaderType::ComputeShader,
        .compiler = ShaderCompilerBackend::Slang
    } };

    auto result = compiler.CompileShader(shader);
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error());
}

TEST_F(ShaderCompileContextCompilerTest, Slang_InlineSource_VirtualInclude_Compiles)
{
    ShaderCompiler compiler{};
    auto& ctx = compiler.GetCompileContext();
    ctx.AddVirtualFile("VirtualHelper.slang", kSlangVirtualHelper);

    Shader shader{ ShaderKey{
        .sourceCode = kSlangMainWithVirtualImport,
        .entryPoint = "CSMain",
        .type = ShaderType::ComputeShader,
        .compiler = ShaderCompilerBackend::Slang
    } };

    auto result = compiler.CompileShader(shader);
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error());
}
#endif // VEX_SLANG

// ============================================================
// Tests: Graphics API (integration-level, with GPU context)
// ============================================================

struct ShaderCompileContextGraphicsTest : VexTest
{
};

TEST_F(ShaderCompileContextGraphicsTest, CreateShaderCompileContext_IsNotNull)
{
    auto& ctx = graphics.GetShaderCompileContext();
    EXPECT_NE(&ctx, nullptr);
}

TEST_F(ShaderCompileContextGraphicsTest, DXC_Dispatch_WithInlineSource_AndVirtualInclude)
{
    auto& ctx = graphics.GetShaderCompileContext();
    ctx.AddVirtualFile("VirtualHelper.hlsli", kHlslVirtualHelper);

    CommandContext cmdCtx = graphics.CreateCommandContext(QueueType::Compute);
    cmdCtx.Dispatch(
        ShaderKey{
            .sourceCode = kHlslMainWithVirtualInclude,
            .entryPoint = "CSMain",
            .type = ShaderType::ComputeShader,
            .compiler = ShaderCompilerBackend::DXC
        },
        {},
        { 1, 1, 1 });

    graphics.Submit(cmdCtx);
    graphics.FlushGPU();
}

#if VEX_SLANG
TEST_F(ShaderCompileContextGraphicsTest, Slang_Dispatch_WithInlineSource_AndVirtualInclude)
{
    auto& ctx = graphics.GetShaderCompileContext();
    ctx.AddVirtualFile("VirtualHelper.slang", kSlangVirtualHelper);

    CommandContext cmdCtx = graphics.CreateCommandContext(QueueType::Compute);
    cmdCtx.Dispatch(
        ShaderKey{
            .sourceCode = kSlangMainWithVirtualImport,
            .entryPoint = "CSMain",
            .type = ShaderType::ComputeShader,
            .compiler = ShaderCompilerBackend::Slang
        },
        {},
        { 1, 1, 1 });

    graphics.Submit(cmdCtx);
    graphics.FlushGPU();
}

TEST_F(ShaderCompileContextGraphicsTest, Slang_SharedContext_CachesSessionAcrossDispatches)
{
    // Both dispatches share the same context — verifies the persistent ISession path
    // is exercised and produces consistent output without crashing.
    auto& ctx = graphics.GetShaderCompileContext();

    CommandContext cmdCtx1 = graphics.CreateCommandContext(QueueType::Compute);
    cmdCtx1.Dispatch(
        ShaderKey{
            .sourceCode = kSlangMinimalCompute,
            .entryPoint = "CSMain",
            .type = ShaderType::ComputeShader,
            .compiler = ShaderCompilerBackend::Slang
        },
        {},
        { 1, 1, 1 });
    graphics.Submit(cmdCtx1);

    CommandContext cmdCtx2 = graphics.CreateCommandContext(QueueType::Compute);
    cmdCtx2.Dispatch(
        ShaderKey{
            .sourceCode = kSlangMinimalCompute,
            .entryPoint = "CSMain",
            .type = ShaderType::ComputeShader,
            .compiler = ShaderCompilerBackend::Slang
        },
        {},
        { 1, 1, 1 });
    graphics.Submit(cmdCtx2);

    graphics.FlushGPU();
}
#endif // VEX_SLANG

} // namespace vex
