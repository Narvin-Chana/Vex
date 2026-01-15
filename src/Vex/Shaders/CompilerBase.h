#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <Vex/Formats.h>
#include <Vex/Types.h>

namespace vex
{

class Shader;
struct ShaderCompilerSettings;
struct ShaderEnvironment;

struct ShaderReflection
{
    struct Input
    {
        std::string semanticName;
        u32 semanticIndex;
        TextureFormat format;

        constexpr bool operator==(const Input& other) const = default;
    };
    std::vector<Input> inputs;
    // Add other reflection data here

    constexpr bool operator==(const ShaderReflection& other) const = default;
};

struct ShaderCompilationResult
{
    SHA1HashDigest sourceHash = {};
    std::vector<byte> compiledCode;
    std::optional<ShaderReflection> reflection;
};

struct CompilerBase
{
    CompilerBase() = default;
    CompilerBase(std::vector<std::filesystem::path> includeDirectories)
        : includeDirectories(std::move(includeDirectories))
    {
    }
    virtual ~CompilerBase() = default;

    virtual std::expected<SHA1HashDigest, std::string> GetShaderCodeHash(
        const Shader& shader, const ShaderEnvironment& shaderEnv, const ShaderCompilerSettings& compilerSettings) = 0;
    virtual std::expected<ShaderCompilationResult, std::string> CompileShader(
        const Shader& shader,
        const ShaderEnvironment& shaderEnv,
        const ShaderCompilerSettings& compilerSettings) const = 0;

protected:
    std::vector<std::filesystem::path> includeDirectories;
};

} // namespace vex