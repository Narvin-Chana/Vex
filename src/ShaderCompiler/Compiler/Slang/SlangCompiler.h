#pragma once

#include <slang-com-ptr.h>
#include <slang.h>

#include <Vex/Utility/NonNullPtr.h>

#include <ShaderCompiler/Compiler/CompilerBase.h>

namespace vex::sc
{
struct ShaderKey;
struct ShaderDefine;

struct SlangCompiler : public CompilerBase
{
    SlangCompiler() = default;
    explicit SlangCompiler(std::vector<std::filesystem::path> incDirs);

    virtual std::expected<ShaderCompilationResult, std::string> CompileShader(
        const Shader& shader,
        const std::filesystem::path& filepath,
        const ShaderEnvironment& environment,
        const ShaderCompilerSettings& compilerSettings) override;
    virtual std::expected<ShaderCompilationResult, std::string> CompileShader(
        const Shader& shader,
        std::string_view sourceCode,
        const ShaderEnvironment& environment,
        const ShaderCompilerSettings& compilerSettings) override;

private:
    static SHA1HashDigest GetShaderCodeHash(const Slang::ComPtr<slang::IComponentType>& linkedShaderProgram);
    static std::expected<ShaderCompilationResult, std::string> CompileFromModule(
        const Shader& shader, const Slang::ComPtr<slang::ISession>& session, NonNullPtr<slang::IModule> module);

    void FillInIncludeDirectories(std::vector<std::string>& includeDirStrings,
                                  std::vector<const char*>& includeDirCStr,
                                  slang::SessionDesc& desc) const;

    std::expected<Slang::ComPtr<slang::ISession>, std::string> CreateSession(
        const ShaderKey& key, const ShaderEnvironment& shaderEnv, const ShaderCompilerSettings& compilerSettings) const;

    Slang::ComPtr<slang::IGlobalSession> globalSession;
};

} // namespace vex::sc