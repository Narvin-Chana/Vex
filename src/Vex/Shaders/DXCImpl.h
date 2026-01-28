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
} // namespace vex

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

#include <Vex/Shaders/CompilerBase.h>

namespace vex
{
struct ShaderKey;
struct ShaderDefine;

struct DXCCompilerImpl : public CompilerBase
{
    DXCCompilerImpl(std::vector<std::filesystem::path> includeDirectories = {});
    virtual ~DXCCompilerImpl() override;

    virtual std::expected<SHA1HashDigest, std::string> GetShaderCodeHash(
        const Shader& shader,
        const ShaderEnvironment& shaderEnv,
        const ShaderCompilerSettings& compilerSettings) override;
    virtual std::expected<ShaderCompilationResult, std::string> CompileShader(
        const Shader& shader,
        const ShaderEnvironment& shaderEnv,
        const ShaderCompilerSettings& compilerSettings) const override;

private:
    void FillInIncludeDirectories(std::vector<LPCWSTR>& args, std::vector<std::wstring>& wStrings) const;

    std::expected<ComPtr<IDxcResult>, std::string> CompileShader(
        const ShaderKey& key,
        const std::vector<std::wstring>& args,
        const std::vector<std::pair<std::wstring, std::wstring>>& defines,
        const DxcBuffer& shaderSource) const;

    ComPtr<IDxcCompiler3> compiler;
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcIncludeHandler> defaultIncludeHandler;
};

} // namespace vex