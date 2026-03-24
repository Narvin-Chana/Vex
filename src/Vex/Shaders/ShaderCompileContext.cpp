#include "ShaderCompileContext.h"

namespace vex
{

ShaderCompileContext::ShaderCompileContext() = default;
ShaderCompileContext::~ShaderCompileContext() = default;

void ShaderCompileContext::AddVirtualFile(const std::string& virtualPath, const std::string& sourceCode)
{
    virtualFiles[virtualPath] = sourceCode;
}

std::optional<const std::string> ShaderCompileContext::GetVirtualFile(const std::string& path) const
{
    auto it = virtualFiles.find(path);

    if (it != virtualFiles.end())
        return it->second;

    return "";
}

std::optional<const std::string> ShaderCompileContext::GetVirtualFile(const std::filesystem::path path) const
{
    return GetVirtualFile(path.string());
}

} // namespace vex
