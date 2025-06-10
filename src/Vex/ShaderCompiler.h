#pragma once

#include <expected>
#include <utility>
#include <vector>

// Include dependencies required by DXC.
#if defined(_WIN32)
#include <windows.h>
#include <wrl/client.h>

#include <Unknwn.h>
#include <dxcapi.h>

namespace vex
{
template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
}

#elif defined(__linux__)
// DXC exposes an adapter for non-windows platforms.
#include <dxc/WinAdapter.h>
#include <dxc/dxcapi.h>

namespace vex
{
template <class T>
using ComPtr = CComPtr<T>;
}

#endif

#include <Vex/RHI/RHIFwd.h>
#include <Vex/ShaderKey.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

struct CompilerUtil
{
    CompilerUtil();
    ~CompilerUtil();

    ComPtr<IDxcCompiler3> compiler;
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcIncludeHandler> defaultIncludeHandler;
};

struct ShaderCache
{
    ShaderCache() = default;
    ShaderCache(RHI* rhi, bool enableShaderDebugging);
    ~ShaderCache();

    ShaderCache(const ShaderCache&) = delete;
    ShaderCache& operator=(const ShaderCache&) = delete;

    ShaderCache(ShaderCache&&) = default;
    ShaderCache& operator=(ShaderCache&&) = default;

    std::expected<void, std::string> CompileShader(RHIShader& shader);
    RHIShader* GetShader(const ShaderKey& key);

    // Checks if the shader's hash is different compared to the last time it was compiled. Returns if the shader is
    // stale or not and the shader's latest hash (which can potentially be the same as the original).
    std::pair<bool, std::size_t> IsShaderStale(const RHIShader& shader) const;

    void MarkShaderDirty(const ShaderKey& key);
    void MarkAllShadersDirty();

    // Marks all stale shaders as dirty and thus in need of recompilation.
    void MarkAllStaleShadersDirty();

private:
    // Obtains a hash of the preprocessed shader, allowing us to verify if the shader's content has changed.
    std::optional<std::size_t> GetShaderHash(const RHIShader& shader) const;
    ComPtr<IDxcResult> GetPreprocessedShader(const RHIShader& shader, const ComPtr<IDxcBlobEncoding>& shaderBlob) const;
    void FillInAdditionalIncludeDirectories(std::vector<LPCWSTR>& args) const;

    static thread_local CompilerUtil GCompilerUtil;

    RHI* rhi;
    // Determines if shaders should be compiled with debug symbols.
    // Defaults to true in non-shipping builds and false in shipping.
    bool debugShaders = !VEX_SHIPPING;
    std::vector<std::filesystem::path> additionalIncludeDirectories;

    std::unordered_map<ShaderKey, UniqueHandle<RHIShader>> shaderCache;
};

} // namespace vex