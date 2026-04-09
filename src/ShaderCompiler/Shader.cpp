#include "Shader.h"

namespace vex::sc
{

Shader::Shader(const ShaderKey& key)
    : key{ key }
    , name{ std::format("{}", key) }
{
}

Shader::~Shader() = default;

const ShaderKey& Shader::GetKey() const
{
    return key;
}

const SHA1HashDigest& Shader::GetHash() const
{
    return res.sourceHash;
}

Span<const byte> Shader::GetBlob() const
{
    return res.compiledCode;
}

void Shader::SetBlob(Span<const byte> blob)
{
    res.compiledCode = { blob.begin(), blob.end() };
}

bool Shader::IsValid() const
{
    return !res.compiledCode.empty();
}

const ShaderReflection* Shader::GetReflection() const
{
    return res.reflection ? &*res.reflection : nullptr;
}

void Shader::SetReflection(ShaderReflection* reflection)
{
    res.reflection.emplace(*reflection);
}

Shader::operator ShaderView() const
{
    ShaderView view{ name, key.entryPoint, GetBlob(), GetHash(), key.type };
    if (isErrored)
    {
        return ShaderView::CreateErroredShader(std::move(view));
    }
    return view;
}
} // namespace vex::sc