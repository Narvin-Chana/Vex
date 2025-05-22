#include "ShaderCompiler.h"

#include <Vex/Logger.h>
#include <Vex/Platform/Windows/WString.h>

namespace vex
{

CompilerUtil::CompilerUtil()
{
    // Create compiler and utils.
    HRESULT hr1 = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.get()));
    HRESULT hr2 = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.get()));

    if (FAILED(hr1) || FAILED(hr2))
    {
        VEX_LOG(Fatal, "Failed to create DxcCompiler and/or DxcUtils...");
    }

    // Create default include handler.
    HRESULT hr3 = (*utils)->CreateDefaultIncludeHandler(defaultIncludeHandler.get());

    if (FAILED(hr3))
    {
        VEX_LOG(Fatal, "Failed to create default include handler...");
    }
}

IDxcCompiler3* CompilerUtil::GetCompiler()
{
    return *compiler;
}

IDxcUtils* CompilerUtil::GetUtils()
{
    return *utils;
}

IDxcIncludeHandler* CompilerUtil::GetDefaultIncludeHandler()
{
    return *defaultIncludeHandler;
}

thread_local CompilerUtil GCompilerUtil;

std::expected<IDxcBlob*, std::string> ShaderCompiler::CompileShader(RHIShader& shader)
{
    DXCHandle<IDxcBlobEncoding> shaderBlob;
    HRESULT hr = GCompilerUtil.GetUtils()->LoadFile(shader.key.path.wstring().c_str(), nullptr, shaderBlob.get());
    DxcBuffer shaderSource = {};
    shaderSource.Ptr = (*shaderBlob)->GetBufferPointer();
    shaderSource.Size = (*shaderBlob)->GetBufferSize();
    shaderSource.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.

    // clang-format off
    std::vector<LPCWSTR> args = {
        L"-Qstrip_reflect",
    };
    // clang-format on

    if (debugShaders)
    {
        args.insert(args.end(),
                    {
                        DXC_ARG_DEBUG,
                        DXC_ARG_WARNINGS_ARE_ERRORS,
                        DXC_ARG_DEBUG_NAME_FOR_SOURCE,
                        L"-Qembed_debug",
                    });
    }

    for (auto& additionalIncludeFolder : additionalIncludeDirectories)
    {
        args.insert(args.end(), { L"-I", StringToWString(additionalIncludeFolder.string()).c_str() });
    }

    DXCHandle<IDxcCompilerArgs> compilerArgs;
    // TODO: pass in feature checker
    // TODO: convert defines to DXCDefine format
    HRESULT hr = GCompilerUtil.GetUtils()->BuildArguments(shader.key.path.wstring().c_str(),
                                                          StringToWString(shader.key.entryPoint).c_str(),
                                                          GetTargetFromShaderType(shader.key.type).c_str(),
                                                          args.data(),
                                                          args.size(),
                                                          ConvertToDXCDefine(shader.key.defines).data(),
                                                          static_cast<u32>(shader.key.defines.size()),
                                                          compilerArgs.get());

    // Compile shader

    // Generate reflection info

    //

    return std::unexpected("");
}

} // namespace vex