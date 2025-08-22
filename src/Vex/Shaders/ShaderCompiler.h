#pragma once

#include <array>
#include <expected>
#include <utility>
#include <vector>

#include <Vex/RHIFwd.h>
#include <Vex/Shaders/CompilerInterface.h>
#include <Vex/Shaders/ShaderCompilerSettings.h>
#include <Vex/Shaders/ShaderKey.h>
#include <Vex/UniqueHandle.h>
#include <Vex/NonNullPtr.h>

namespace vex
{

struct ShaderResourceContext;
class Shader;
struct ShaderEnvironment;

using ShaderCompileErrorsCallback = bool(const std::vector<std::pair<ShaderKey, std::string>>& errors);

struct ShaderCompiler
{
    ShaderCompiler();
    ShaderCompiler(RHI* rhi, const ShaderCompilerSettings& compilerSettings);
    ~ShaderCompiler();

    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;

    ShaderCompiler(ShaderCompiler&&) = default;
    ShaderCompiler& operator=(ShaderCompiler&&) = default;

    std::expected<void, std::string> CompileShader(Shader& shader, const ShaderResourceContext& resourceContext);
    NonNullPtr<Shader> GetShader(const ShaderKey& key, const ShaderResourceContext& resourceContext);

    // Checks if the shader's hash is different compared to the last time it was compiled. Returns if the shader is
    // stale or not and the shader's latest hash (which can potentially be the same as the original).
    std::pair<bool, std::size_t> IsShaderStale(const Shader& shader) const;

    void MarkShaderDirty(const ShaderKey& key);
    void MarkAllShadersDirty();

    // Marks all stale shaders as dirty and thus in need of recompilation.
    void MarkAllStaleShadersDirty();

    void SetCompilationErrorsCallback(std::function<ShaderCompileErrorsCallback> callback);
    void FlushCompilationErrors();

private:
    std::expected<std::string, std::string> ProcessShaderCodeGen(const std::string& shaderFileStr,
                                                                 const ShaderResourceContext& resourceContext);

    ShaderEnvironment CreateShaderEnvironment();

    RHI* rhi;
    ShaderCompilerSettings compilerSettings;
    UniqueHandle<CompilerInterface> compilerImpl;

    std::vector<std::filesystem::path> additionalIncludeDirectories;

    std::unordered_map<ShaderKey, Shader> shaderCache;

    std::function<ShaderCompileErrorsCallback> errorsCallback = nullptr;
    std::vector<std::pair<ShaderKey, std::string>> compilationErrors;
};

} // namespace vex