#pragma once

#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <Vex/Shaders/CompilerBase.h>

namespace vex
{

struct ShaderDefine;

struct SlangCompilerImpl : public CompilerBase
{
    SlangCompilerImpl() = default;
    SlangCompilerImpl(std::vector<std::filesystem::path> incDirs);
    virtual ~SlangCompilerImpl() override;

    virtual std::expected<ShaderCompilationResult, std::string> CompileShader(
        const Shader& shader,
        ShaderEnvironment& shaderEnv,
        const ShaderCompilerSettings& compilerSettings) const override;

private:
    void FillInIncludeDirectories(std::vector<std::string>& includeDirStrings,
                                  std::vector<const char*>& includeDirCStr,
                                  slang::SessionDesc& desc) const;
    Slang::ComPtr<slang::ISession> CreateSession(const Shader& shader,
                                                 ShaderEnvironment& shaderEnv,
                                                 const ShaderCompilerSettings& compilerSettings) const;

    Slang::ComPtr<slang::IGlobalSession> globalSession;
    Slang::ComPtr<slang::IModule> module;
};

} // namespace vex