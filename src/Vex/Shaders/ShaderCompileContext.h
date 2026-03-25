#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace vex
{

struct ICompilerContextImpl
{
    virtual ~ICompilerContextImpl() = default;
    virtual bool LoadModule(const std::string& moduleName) = 0;
};

struct ShaderCompiler;
struct DXCCompilerImpl;

#if VEX_SLANG
struct SlangCompilerImpl;
struct SlangCompilerContextImpl;
#endif

class ShaderCompileContext
{
public:
    ShaderCompileContext();
    ~ShaderCompileContext();

    ShaderCompileContext(const ShaderCompileContext&) = delete;
    ShaderCompileContext& operator=(const ShaderCompileContext&) = delete;

    ShaderCompileContext(ShaderCompileContext&&) = default;
    ShaderCompileContext& operator=(ShaderCompileContext&&) = default;

    // Registers a virtual file in the CompileContext's Virtual File System (VFS).
    // This allows #include in DXC and import/#include in Slang to resolve the provided source code,
    // even if it does not exist on disk.
    void AddVirtualFile(const std::string& virtualPath, const std::string& sourceCode);

    [[nodiscard]] const std::unordered_map<std::string, std::string>& GetVirtualFiles() const
    {
        return virtualFiles;
    }

    std::optional<const std::string> GetVirtualFile(const std::filesystem::path& path) const;

private:
    std::unordered_map<std::string, std::string> virtualFiles;

    friend ShaderCompiler;
    friend DXCCompilerImpl;

#if VEX_SLANG
    friend SlangCompilerImpl;
    std::unique_ptr<SlangCompilerContextImpl> slangImpl;
#endif
};

} // namespace vex
