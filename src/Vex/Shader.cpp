#include "Shader.h"

namespace vex
{

Shader::Shader(const ShaderKey& key)
    : key{ key }
{
}

Shader::~Shader()
{
}

std::span<const u8> Shader::GetBlob() const
{
    return blob;
}

bool Shader::IsValid() const
{
    return !blob.empty();
}

bool Shader::NeedsRecompile() const
{
    return isDirty && !isErrored;
}

void Shader::MarkDirty()
{
    isDirty = true;
}

} // namespace vex