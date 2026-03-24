#include "ShaderCompileContext.h"

namespace vex
{

ShaderCompileContext::ShaderCompileContext() = default;
ShaderCompileContext::~ShaderCompileContext() = default;

void ShaderCompileContext::AddVirtualFile(const std::string& virtualPath, const std::string& sourceCode)
{
    virtualFiles[virtualPath] = sourceCode;
}

std::string ShaderCompileContext::GetVirtualFile(const std::string& path)
{
    auto it = virtualFiles.find(path);

    if (it != virtualFiles.end())
        return it->second;

    return "";
}

} // namespace vex
