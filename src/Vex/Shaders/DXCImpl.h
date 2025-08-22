#pragma once

#include <array>
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

#include <Vex/Shaders/CompilerInterface.h>

namespace vex
{

struct DXCCompilerImpl : public CompilerInterface
{
    DXCCompilerImpl();
    virtual ~DXCCompilerImpl() override;

    // Obtains a hash of the preprocessed shader, allowing us to verify if the shader's content has changed.
    virtual std::optional<std::size_t> GetShaderHash(
        const Shader& shader, std::span<const std::filesystem::path> additionalIncludeDirectories) const override;

    virtual std::expected<std::vector<u8>, std::string> CompileShader(
        std::string& shaderFileStr,
        ShaderEnvironment& shaderEnv,
        std::span<const std::filesystem::path> additionalIncludeDirectories,
        const ShaderCompilerSettings& compilerSettings,
        const Shader& shader) const override;

private:
    ComPtr<IDxcResult> GetPreprocessedShader(const Shader& shader,
                                             const ComPtr<IDxcBlobEncoding>& shaderBlob,
                                             std::span<const std::filesystem::path> additionalIncludeDirectories) const;

    ComPtr<IDxcCompiler3> compiler;
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcIncludeHandler> defaultIncludeHandler;
};

} // namespace vex