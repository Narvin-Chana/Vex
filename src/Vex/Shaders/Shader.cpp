#include "Shader.h"

namespace vex
{

Shader::Shader(const ShaderKey& key)
    : key{ key }
{
}

Shader::~Shader() = default;

std::span<const byte> Shader::GetBlob() const
{
    return res.compiledCode;
}

bool Shader::IsValid() const
{
    return !res.compiledCode.empty();
}

bool Shader::NeedsRecompile() const
{
    return isDirty && !isErrored;
}
const ShaderReflection& Shader::GetReflection() const
{
    return res.reflection;
}

void Shader::MarkDirty()
{
    isDirty = true;
}

} // namespace vex