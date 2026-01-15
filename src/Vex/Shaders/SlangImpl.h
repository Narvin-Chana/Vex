#pragma once

#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <Vex/Shaders/CompilerBase.h>

namespace vex
{
struct ShaderKey;
struct ShaderDefine;

struct SlangCompilerImpl : public CompilerBase
{
    SlangCompilerImpl() = default;
    SlangCompilerImpl(std::vector<std::filesystem::path> incDirs);
    virtual ~SlangCompilerImpl() override;

    virtual std::expected<SHA1HashDigest, std::string> GetShaderCodeHash(
        const Shader& shader,
        const ShaderEnvironment& shaderEnv,
        const ShaderCompilerSettings& compilerSettings) override;
    virtual std::expected<ShaderCompilationResult, std::string> CompileShader(
        const Shader& shader,
        const ShaderEnvironment& shaderEnv,
        const ShaderCompilerSettings& compilerSettings) const override;

private:
    void FillInIncludeDirectories(std::vector<std::string>& includeDirStrings,
                                  std::vector<const char*>& includeDirCStr,
                                  slang::SessionDesc& desc) const;

    std::expected<Slang::ComPtr<slang::ISession>, std::string> CreateSession(
        const ShaderKey& key, const ShaderEnvironment& shaderEnv, const ShaderCompilerSettings& compilerSettings) const;

    void ValidateShaderForCompilation(const Shader& shader) const;

    Slang::ComPtr<slang::IGlobalSession> globalSession;
};

} // namespace vex