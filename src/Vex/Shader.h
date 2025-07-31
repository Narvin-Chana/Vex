#pragma once

#include <span>
#include <vector>

#include <Vex/ShaderKey.h>
#include <Vex/Types.h>

namespace vex
{

class Shader
{
public:
    Shader(const ShaderKey& key);
    ~Shader();

    std::span<const u8> GetBlob() const;
    bool IsValid() const;
    bool NeedsRecompile() const;

    void MarkDirty();

    ShaderKey key;
    u32 version = 0;

private:
    bool isDirty = true;
    // Errored shaders are set in stasis while awaiting a confirmation of whether to launch a recompilation.
    bool isErrored = false;
    std::vector<u8> blob;
    std::size_t hash = 0;

    friend struct ShaderCompiler;
};

} // namespace vex