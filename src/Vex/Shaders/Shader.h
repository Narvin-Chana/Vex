#pragma once

#include <filesystem>
#include <span>
#include <unordered_set>
#include <vector>

#include <Vex/Shaders/ShaderCompiler.h>
#include <Vex/Shaders/ShaderKey.h>
#include <Vex/Types.h>

namespace vex
{

class Shader
{
public:
    Shader(const ShaderKey& key);
    ~Shader();

    std::span<const byte> GetBlob() const;
    bool IsValid() const;
    bool NeedsRecompile() const;
    const ShaderReflection* GetReflection() const;

    void MarkDirty();

    ShaderKey key;
    u32 version = 0;

private:
    bool isDirty = true;
    // Errored shaders are set in stasis while awaiting a confirmation of whether to launch a recompilation.
    bool isErrored = false;
    ShaderCompilationResult res;

    friend struct ShaderCompiler;
    friend struct SlangCompilerImpl;
};

} // namespace vex