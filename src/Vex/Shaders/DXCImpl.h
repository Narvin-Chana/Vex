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

#include <Vex/Shaders/CompilerBase.h>

namespace vex
{

struct ShaderDefine;

struct DXCCompilerImpl : public CompilerBase
{
    DXCCompilerImpl(std::vector<std::filesystem::path> includeDirectories);
    virtual ~DXCCompilerImpl() override;

    virtual std::expected<std::vector<u8>, std::string> CompileShader(
        const Shader& shader,
        ShaderEnvironment& shaderEnv,
        const ShaderCompilerSettings& compilerSettings,
        const ShaderResourceContext& resourceContext) const override;

private:
    void FillInIncludeDirectories(std::vector<LPCWSTR>& args) const;
    void GenerateVexBindings(std::vector<ShaderDefine>& defines, const ShaderResourceContext& resourceContext) const;

    ComPtr<IDxcCompiler3> compiler;
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcIncludeHandler> defaultIncludeHandler;
};

} // namespace vex