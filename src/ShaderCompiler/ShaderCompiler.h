#pragma once

#include <utility>

#ifndef __cpp_lib_move_only_function
// Fallback for environments with incomplete C++23 support
#include <Vex/Utility/Functional/move_only_function.h>
#endif

#include <Vex/Containers/Span.h>
#include <Vex/ShaderView.h>
#include <Vex/Utility/MoveOnlyFunction.h>
#include <Vex/Utility/NonNullPtr.h>

#include <ShaderCompiler/Compiler/CompilerBase.h>
#include <ShaderCompiler/ShaderCompilerSettings.h>
#include <ShaderCompiler/ShaderEnvironment.h>
#include <ShaderCompiler/ShaderKey.h>

#if VEX_DXC
#include <ShaderCompiler/Compiler/DXC/DXCCompiler.h>
#endif
#if VEX_SLANG
#include <ShaderCompiler/Compiler/Slang/SlangCompiler.h>
#endif

namespace vex
{
struct RayTracingShaderCollection;
}
namespace vex::sc
{

namespace ShaderUtil
{
bool IsBuiltInSemantic(std::string_view name);
bool CanReflectShaderType(ShaderType type);
} // namespace ShaderUtil

// Determines the response when a shader compilation fails.
enum class ShaderHotReloadErrorResponse : u8
{
    // Retry compilation, retaining the previous shader (if any exists) in the meantime.
    RetryAndRetainPreviousShader,
    // Will keep the previous valid shader (if any exists), without attempting to recompile (meaning the shader will
    // only be recompiled when manually called).
    RetainPreviousShader,
    // Will fatally exit the application.
    Abort,
};

using ShaderHotReloadErrorsCallbackFuncType =
    ShaderHotReloadErrorResponse(Span<const std::pair<ShaderKey, std::string>>);
using ShaderHotReloadErrorsCallback = MoveOnlyFunction<ShaderHotReloadErrorsCallbackFuncType>;

struct RayTracingShaderKey;

struct ShaderCompiler
{
    ShaderCompiler(const ShaderCompilerSettings& compilerSettings = {});
    ShaderCompiler(ShaderCompiler&&) = default;
    ShaderCompiler& operator=(ShaderCompiler&&) = default;
    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;
    ~ShaderCompiler();

    // Obtains the shader view from the specified shader key. Will create and compile the shader if shader hot-reload is
    // enabled.
    ShaderView GetShaderView(const ShaderKey& key);

    // Obtains the shader from the specified shader key. Will create and compile the shader if shader hot-reload is
    // enabled.
    NonNullPtr<Shader> GetShader(const ShaderKey& key);

    // Allows the user to obtain a raytracing shader collection from the provided ray tracing shader key.
    RayTracingShaderCollection GetRayTracingShaderCollection(const RayTracingShaderKey& rtShaderKey);

    // Will register and compile a shader from filesystem path.
    std::optional<std::string> CompileShaderFromFilepath(const ShaderKey& key);

    // Will register and compile a shader directly from source code. You must provide a virtual filepath in the key
    // which will be used to get the compiled shader.
    std::optional<std::string> CompileShaderFromSourceCode(const ShaderKey& key, std::string_view sourceCode);

    // Will be called when shader hot-reload is enabled and shader compilation results in an error.
    void SetShaderCompilationErrorsCallback(ShaderHotReloadErrorsCallback&& callback);

    // Recompiles all filepath-based shaders which have changed since the last compilation. Useful for shader
    // development and hot-reloading. You generally want to avoid calling this too often if your application has many
    // shaders. Only valid if enableShaderHotReload is enabled.
    void RecompileChangedShaders();

    // Recompiles all filepath-based shaders, could cause a big hitch depending on how many shaders your application
    // uses. Only valid if enableShaderHotReload is enabled.
    void RecompileAllShaders();

    // Recompiles all filepath-based shaders passed-in.
    void RecompileShaders(Span<const ShaderKey> shaderKeys);

private:
    static std::optional<std::filesystem::path> TryGetFilepathFromVirtualFilepath(const ShaderKey& key);
    static ShaderEnvironment CreateShaderEnvironment(const ShaderCompilerSettings& compilerSettings);

    CompilerBase& GetCompiler(const ShaderKey& key);
    NonNullPtr<Shader> GetShader(const ShaderKey& key, bool allowHotReload);
    std::optional<std::string> HandleCompiledShader(
        Shader& shader, std::expected<ShaderCompilationResult, std::string>&& compilationResult) const;
    bool IsShaderStale(const Shader& shader) const;
    bool HandleCompilationErrors(Span<const std::pair<ShaderKey, std::string>> shaderKeysAndErrors);

    template <std::ranges::input_range TRange>
        requires std::same_as<std::ranges::range_value_t<TRange>, ShaderKey>
    u32 HotReloadShaders(TRange&& shaderKeys);

    ShaderCompilerSettings compilerSettings;
#if VEX_DXC
    DXCCompiler dxcCompiler;
#endif
#if VEX_SLANG
    SlangCompiler slangCompiler;
#endif
    ShaderEnvironment globalShaderEnv;
    std::unordered_map<ShaderKey, Shader> shaderCache;

    ShaderHotReloadErrorsCallback errorsCallback;
};

} // namespace vex::sc