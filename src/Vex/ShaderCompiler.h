#pragma once

#include <expected>
#include <utility>
#include <vector>

// Include dependencies required by DXC.
#if defined(_WIN32)
#include <Unknwn.h>
#include <dxcapi.h>
#include <windows.h>
#include <wrl/client.h>

namespace vex
{
template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
}

#elif defined(__linux__)
#define __EMULATE_UUID 1
// DXC exposes an adapter for non-windows platforms.
#include <dxc/WinAdapter.h>
#include <dxc/dxcapi.h>

namespace vex
{
template <class T>
using ComPtr = CComPtr<T>;
}

#endif

#include <Vex/RHIFwd.h>
#include <Vex/ShaderCompilerSettings.h>
#include <Vex/ShaderKey.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

struct ShaderResourceContext;
class Shader;

struct CompilerUtil
{
    CompilerUtil();
    ~CompilerUtil();

    ComPtr<IDxcCompiler3> compiler;
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcIncludeHandler> defaultIncludeHandler;
};

using ShaderCompileErrorsCallback = bool(const std::vector<std::pair<ShaderKey, std::string>>& errors);

struct ShaderCompiler
{
    ShaderCompiler() = default;
    ShaderCompiler(RHI* rhi, const ShaderCompilerSettings& compilerSettings);
    ~ShaderCompiler();

    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;

    ShaderCompiler(ShaderCompiler&&) = default;
    ShaderCompiler& operator=(ShaderCompiler&&) = default;

    std::expected<void, std::string> CompileShader(Shader& shader, const ShaderResourceContext& resourceContext);
    Shader* GetShader(const ShaderKey& key, const ShaderResourceContext& resourceContext);

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
    static constexpr auto HLSL202xFlags = {
        L"-HV 202x", L"-Wconversion", L"-Wdouble-promotion", L"-Whlsl-legacy-literal"
    };

    // Obtains a hash of the preprocessed shader, allowing us to verify if the shader's content has changed.
    std::optional<std::size_t> GetShaderHash(const Shader& shader) const;
    ComPtr<IDxcResult> GetPreprocessedShader(const Shader& shader, const ComPtr<IDxcBlobEncoding>& shaderBlob) const;
    void FillInAdditionalIncludeDirectories(std::vector<LPCWSTR>& args) const;

    static CompilerUtil& GetCompilerUtil();

    RHI* rhi;
    ShaderCompilerSettings compilerSettings;

    std::vector<std::filesystem::path> additionalIncludeDirectories;

    std::unordered_map<ShaderKey, UniqueHandle<Shader>> shaderCompiler;

    std::function<ShaderCompileErrorsCallback> errorsCallback = nullptr;
    std::vector<std::pair<ShaderKey, std::string>> compilationErrors;
};

} // namespace vex