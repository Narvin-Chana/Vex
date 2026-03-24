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

    std::optional<const std::string> GetVirtualFile(const std::string& path) const;
    std::optional<const std::string> GetVirtualFile(const std::filesystem::path path) const;

    template <typename T>
    T* GetImpl() const
    {
        return static_cast<T*>(opaqueImpl.get());
    }

    void SetImpl(std::unique_ptr<ICompilerContextImpl> impl)
    {
        opaqueImpl = std::move(impl);
    }

private:
    std::unordered_map<std::string, std::string> virtualFiles;
    std::unique_ptr<ICompilerContextImpl> opaqueImpl;
};

} // namespace vex
