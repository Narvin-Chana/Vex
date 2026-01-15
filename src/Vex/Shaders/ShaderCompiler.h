#pragma once

#include <expected>
#include <utility>
#include <vector>

#include <Vex/Shaders/DXCImpl.h>
#include <Vex/Shaders/ShaderCompilerSettings.h>
#include <Vex/Shaders/ShaderEnvironment.h>
#include <Vex/Shaders/ShaderKey.h>
#include <Vex/Utility/NonNullPtr.h>

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
    std::expected<void, std::string> CompileShader(CompilerBase* Compiler, Shader& shader);
    std::expected<CompilerBase*, std::string> GetCompiler(const ShaderKey& key);

    bool IsShaderStale(const Shader& shader) const;
    static ShaderEnvironment CreateShaderEnvironment();

    ShaderCompilerSettings compilerSettings;
    DXCCompilerImpl dxcCompilerImpl;
#if VEX_SLANG
    SlangCompilerImpl slangCompilerImpl;
#endif

    ShaderEnvironment globalShaderEnv;

    std::unordered_map<ShaderKey, Shader> shaderCache;

    std::function<ShaderCompileErrorsCallback> errorsCallback = nullptr;
    std::vector<std::pair<ShaderKey, std::string>> compilationErrors;
};

} // namespace vex