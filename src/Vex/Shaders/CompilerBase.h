#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <Vex/Types.h>

namespace vex
{

class Shader;
struct ShaderCompilerSettings;
struct ShaderEnvironment;

struct CompilerBase
{
    CompilerBase() = default;
    CompilerBase(std::vector<std::filesystem::path> includeDirectories)
        : includeDirectories(std::move(includeDirectories))
    {
    }
    virtual ~CompilerBase() = default;

    virtual std::expected<std::vector<byte>, std::string> CompileShader(
        const Shader& shader, ShaderEnvironment& shaderEnv, const ShaderCompilerSettings& compilerSettings) const = 0;

protected:
    std::vector<std::filesystem::path> includeDirectories;
};

} // namespace vex