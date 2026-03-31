#pragma once

#include <Vex/Containers/Span.h>
#include <Vex/ShaderView.h>
#include <Vex/Types.h>

#include <ShaderCompiler/Compiler/CompilerBase.h>
#include <ShaderCompiler/ShaderKey.h>

namespace vex::sc
{

class Shader
{
public:
    explicit Shader(const ShaderKey& key);
    ~Shader();

    const ShaderKey& GetKey() const;

    const SHA1HashDigest& GetHash() const;

    Span<const byte> GetBlob() const;
    void SetBlob(Span<const byte> blob);

    bool IsValid() const;

    const ShaderReflection* GetReflection() const;
    void SetReflection(ShaderReflection* reflection);

    explicit operator vex::ShaderView() const;

private:
    ShaderKey key;
    std::string name;
    ShaderCompilationResult res;

    // Set to true when the most recent compilation attempt of this shader failed.
    bool isErrored = false;

    friend class ShaderCompiler;
};

} // namespace vex::sc