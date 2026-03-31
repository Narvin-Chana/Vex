#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <Vex/Formats.h>
#include <Vex/Types.h>

namespace vex::sc
{

class Shader;
struct ShaderEnvironment;
struct ShaderCompilerSettings;

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
    explicit CompilerBase(std::vector<std::filesystem::path> includeDirectories)
        : includeDirectories(std::move(includeDirectories))
    {
    }
    CompilerBase(CompilerBase&&) = default;
    CompilerBase& operator=(CompilerBase&&) = default;
    CompilerBase(const CompilerBase&) = delete;
    CompilerBase& operator=(const CompilerBase&) = delete;
    virtual ~CompilerBase() = default;

    virtual std::expected<ShaderCompilationResult, std::string> CompileShader(
        const Shader& shader,
        const std::filesystem::path& file,
        const ShaderEnvironment& environment,
        const ShaderCompilerSettings& compilerSettings) = 0;
    virtual std::expected<ShaderCompilationResult, std::string> CompileShader(
        const Shader& shader,
        std::string_view sourceCode,
        const ShaderEnvironment& environment,
        const ShaderCompilerSettings& compilerSettings) = 0;

protected:
    std::vector<std::filesystem::path> includeDirectories;
};

} // namespace vex::sc