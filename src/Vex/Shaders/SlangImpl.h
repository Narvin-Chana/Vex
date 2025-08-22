#pragma once

#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <Vex/Shaders/CompilerBase.h>

namespace vex
{

struct SlangCompilerImpl : public CompilerBase
{
    SlangCompilerImpl(std::vector<std::filesystem::path> incDirs);
    virtual ~SlangCompilerImpl() override;

    virtual std::expected<std::vector<u8>, std::string> CompileShader(
        const Shader& shader,
        ShaderEnvironment& shaderEnv,
        const ShaderCompilerSettings& compilerSettings,
        const ShaderResourceContext& resourceContext) const override;

private:
    void FillInIncludeDirectories(std::vector<std::string> includeDirStrings,
                                  std::vector<const char*> includeDirCStr,
                                  slang::SessionDesc& desc);

    Slang::ComPtr<slang::IGlobalSession> globalSession;
    Slang::ComPtr<slang::ISession> session;
    Slang::ComPtr<slang::IModule> module;
};

} // namespace vex