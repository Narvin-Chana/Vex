#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include <slang-com-ptr.h>
#include <slang.h>

#include <Vex/Shaders/CompilerBase.h>
#include <Vex/Shaders/ShaderCompileContext.h>

namespace vex
{
struct ShaderKey;
struct ShaderDefine;

struct SlangCompilerContextImpl : public ICompilerContextImpl
{
    SlangCompilerContextImpl(const ShaderEnvironment& env,
                             const ShaderCompilerSettings& compilerSettings,
                             ShaderCompileContext* parentContext);
    virtual ~SlangCompilerContextImpl() override;

    virtual bool LoadModule(const std::string& moduleName) override;

    Slang::ComPtr<slang::ISession> session;
    ShaderCompileContext* parentContext = nullptr;
};

struct SlangCompilerImpl : public CompilerBase
{
    SlangCompilerImpl() = default;
    SlangCompilerImpl(std::vector<std::filesystem::path> incDirs);
    virtual ~SlangCompilerImpl() override;

    virtual std::unique_ptr<ICompilerContextImpl> CreateContext(const ShaderEnvironment& env,
                                                                const ShaderCompilerSettings& compilerSettings,
                                                                ShaderCompileContext* context) const override;

    virtual std::expected<SHA1HashDigest, std::string> GetShaderCodeHash(
        const Shader& shader,
        const ShaderEnvironment& shaderEnv,
        const ShaderCompilerSettings& compilerSettings,
        ShaderCompileContext* context = nullptr) override;
    virtual std::expected<ShaderCompilationResult, std::string> CompileShader(
        const Shader& shader,
        const ShaderEnvironment& shaderEnv,
        const ShaderCompilerSettings& compilerSettings,
        ShaderCompileContext* context = nullptr) const override;

private:
    void FillInIncludeDirectories(std::vector<std::string>& includeDirStrings,
                                  std::vector<const char*>& includeDirCStr,
                                  slang::SessionDesc& desc) const;

    std::expected<Slang::ComPtr<slang::ISession>, std::string> CreateSession(
        const ShaderKey& key,
        const ShaderEnvironment& shaderEnv,
        const ShaderCompilerSettings& compilerSettings,
        ShaderCompileContext* context = nullptr) const;

    void ValidateShaderForCompilation(const Shader& shader) const;

    Slang::ComPtr<slang::IGlobalSession> globalSession;
};

} // namespace vex