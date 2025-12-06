#pragma once

#include <expected>
#include <utility>
#include <vector>

#include <Vex/Utility/NonNullPtr.h>
#include <Vex/Shaders/DXCImpl.h>
#include <Vex/Shaders/ShaderCompilerSettings.h>
#include <Vex/Shaders/ShaderKey.h>

#include <RHI/RHIFwd.h>

#if VEX_SLANG
#include <Vex/Shaders/SlangImpl.h>
#endif

namespace vex
{

namespace ShaderUtil
{
bool IsBuiltInSemantic(std::string_view name);
bool CanReflectShaderType(ShaderType type);
} // namespace ShaderUtil

class Shader;
struct ShaderEnvironment;

using ShaderCompileErrorsCallback = bool(const std::vector<std::pair<ShaderKey, std::string>>& errors);

struct ShaderCompiler
{
    ShaderCompiler(const ShaderCompilerSettings& compilerSettings = {});
    ~ShaderCompiler();

    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;

    ShaderCompiler(ShaderCompiler&&) = default;
    ShaderCompiler& operator=(ShaderCompiler&&) = default;

    std::expected<void, std::string> CompileShader(Shader& shader);
    NonNullPtr<Shader> GetShader(const ShaderKey& key);

    void MarkShaderDirty(const ShaderKey& key);
    void MarkAllShadersDirty();

    // Marks all stale shaders as dirty and thus in need of recompilation.
    void MarkAllStaleShadersDirty();

    void SetCompilationErrorsCallback(std::function<ShaderCompileErrorsCallback> callback);
    void FlushCompilationErrors();

private:
    // Checks if the shader's hash is different compared to the last time it was compiled. Returns if the shader is
    // stale or not and the shader's latest hash (which can potentially be the same as the original).
    std::pair<bool, std::size_t> IsShaderStale(const Shader& shader) const;
    ShaderEnvironment CreateShaderEnvironment(ShaderCompilerBackend compiler);

    ShaderCompilerSettings compilerSettings;
    DXCCompilerImpl dxcCompilerImpl;
#if VEX_SLANG
    SlangCompilerImpl slangCompilerImpl;
#endif

    std::unordered_map<ShaderKey, Shader> shaderCache;

    std::function<ShaderCompileErrorsCallback> errorsCallback = nullptr;
    std::vector<std::pair<ShaderKey, std::string>> compilationErrors;
};

} // namespace vex