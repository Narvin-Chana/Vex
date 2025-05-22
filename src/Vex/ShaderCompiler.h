#pragma once

#include <expected>

// Include dependencies required by DXC.
#if defined(_WIN32)
#include <windows.h>

#include <Unknwn.h>
#elif defined(__linux__)
// DXC exposes an adapter for non-windows platforms.
#include <WinAdapter.h>
#endif

#include <dxcapi.h>

#include <Vex/RHI/RHIShader.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

template <class T>
struct DXCDeleter
{
    void operator()(T* ptr) const
    {
        (*ptr)->Release();
    };
};

template <typename T>
using DXCHandle = UniqueHandle<T*, DXCDeleter<T*>>;

struct CompilerUtil
{
    CompilerUtil();

    IDxcCompiler3* GetCompiler();
    IDxcUtils* GetUtils();
    IDxcIncludeHandler* GetDefaultIncludeHandler();

private:
    DXCHandle<IDxcCompiler3> compiler;
    DXCHandle<IDxcUtils> utils;
    DXCHandle<IDxcIncludeHandler> defaultIncludeHandler;
};

struct ShaderCompiler
{
    std::expected<IDxcBlob*, std::string> CompileShader(RHIShader& shader);

private:
    // Defaults to true in non-shipping builds and false in shipping.
    bool debugShaders = !VEX_SHIPPING;
    std::vector<std::filesystem::path> additionalIncludeDirectories;
};

} // namespace vex