#include "ShaderCompileContext.h"

namespace vex
{

ShaderCompileContext::ShaderCompileContext() = default;
ShaderCompileContext::~ShaderCompileContext() = default;

void ShaderCompileContext::AddVirtualFile(const std::string& virtualPath, const std::string& sourceCode)
{
    virtualFiles[virtualPath] = sourceCode;
}

bool ShaderCompileContext::LoadSlangModule(const std::string& moduleName)
{
    if (opaqueImpl)
    {
        return opaqueImpl->LoadModule(moduleName);
    }
    return false;
}

} // namespace vex
