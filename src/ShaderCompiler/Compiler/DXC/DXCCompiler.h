#pragma once

#include <vector>

// Include dependencies required by DXC.
#if defined(_WIN32)
#include <Unknwn.h>
#include <dxcapi.h>
#include <windows.h>
#include <wrl/client.h>

namespace vex::sc
{
template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
} // namespace vex::sc

#elif defined(__linux__)
#define __EMULATE_UUID 1
// DXC exposes an adapter for non-windows platforms.
#include <dxc/WinAdapter.h>
#include <dxc/dxcapi.h>

namespace vex::sc
{
template <class T>
using ComPtr = CComPtr<T>;
}
#endif

#include <ShaderCompiler/Compiler/CompilerBase.h>

namespace vex::sc
{
struct ShaderKey;
struct ShaderDefine;

struct DXCCompiler : public CompilerBase
{
    explicit DXCCompiler(std::vector<std::filesystem::path> includeDirectories = {});

    virtual std::expected<ShaderCompilationResult, std::string> CompileShader(
        const Shader& shader,
        const std::filesystem::path& filepath,
        const ShaderEnvironment& environment,
        const ShaderCompilerSettings& compilerSettings) override;
    virtual std::expected<ShaderCompilationResult, std::string> CompileShader(
        const Shader& shader,
        std::string_view sourceCode,
        const ShaderEnvironment& environment,
        const ShaderCompilerSettings& compilerSettings) override;

private:
    std::expected<SHA1HashDigest, std::string> GetShaderCodeHash(const Shader& shader,
                                                                 const DxcBuffer& shaderSource,
                                                                 const ShaderEnvironment& shaderEnv,
                                                                 const ShaderCompilerSettings& compilerSettings) const;
    std::expected<ShaderCompilationResult, std::string> CompileShaderFromBlob(
        const Shader& shader,
        const ComPtr<IDxcBlobEncoding>& shaderSourceBlob,
        const ShaderEnvironment& environment,
        const ShaderCompilerSettings& compilerSettings);
    std::expected<ComPtr<IDxcResult>, std::string> CompileShaderFromBuffer(
        const ShaderKey& key,
        const DxcBuffer& shaderSource,
        const std::vector<std::wstring>& args,
        const std::vector<std::pair<std::wstring, std::wstring>>& defines,
        const ShaderCompilerSettings& compilerSettings) const;

    ComPtr<IDxcCompiler3> compiler;
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcIncludeHandler> defaultIncludeHandler;
};

} // namespace vex::sc