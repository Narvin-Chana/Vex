#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <Vex/Containers/Span.h>
#include <string>

#include <Vex/Types.h>

namespace vex
{

class Shader;
struct ShaderCompilerSettings;
struct ShaderDefine;
struct ShaderEnvironment;

struct CompilerInterface
{
    virtual ~CompilerInterface() = default;
    virtual std::optional<std::size_t> GetShaderHash(
        const Shader& shader, Span<const std::filesystem::path> additionalIncludeDirectories) const = 0;
    virtual std::expected<std::vector<u8>, std::string> CompileShader(
        std::string& shaderFileStr,
        ShaderEnvironment& shaderEnv,
        Span<const std::filesystem::path> additionalIncludeDirectories,
        const ShaderCompilerSettings& compilerSettings,
        const Shader& shader) const = 0;
};

} // namespace vex