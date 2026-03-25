#include "ShaderCompileContext.h"

#if VEX_SLANG
#include <Vex/Shaders/SlangImpl.h>
#endif

namespace vex
{

ShaderCompileContext::ShaderCompileContext()
#if VEX_SLANG
: slangImpl(nullptr)
#endif
{
};

ShaderCompileContext::~ShaderCompileContext() = default;

void ShaderCompileContext::AddVirtualFile(const std::string& virtualPath, const std::string& sourceCode)
{
    virtualFiles[virtualPath] = sourceCode;
}

std::optional<const std::string> ShaderCompileContext::GetVirtualFile(const std::filesystem::path& path) const
{
    std::filesystem::path relPath = path;
    if (path.is_absolute())
    {
        auto relPath = path.lexically_relative(std::filesystem::current_path());
    }
    auto it = virtualFiles.find(relPath.string());

    if (it != virtualFiles.end())
        return it->second;

    return {};
}

} // namespace vex
