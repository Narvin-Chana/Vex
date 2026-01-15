#include "Shader.h"

namespace vex
{

Shader::Shader(const ShaderKey& key)
    : key{ key }
{
}

Shader::~Shader() = default;

Span<const byte> Shader::GetBlob() const
{
    return res.compiledCode;
}

bool Shader::IsValid() const
{
    return !res.compiledCode.empty();
}

const ShaderReflection* Shader::GetReflection() const
{
    return res.reflection ? &*res.reflection : nullptr;
}

void Shader::MarkDirty()
{
    markForRecompile = true;
}

} // namespace vex