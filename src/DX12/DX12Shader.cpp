#include "DX12Shader.h"

namespace vex::dx12
{

DX12Shader::DX12Shader(const ShaderKey& key)
    : RHIShader(key)
{
}

std::span<const u8> DX12Shader::GetBlob() const
{
    return std::span<const u8>(TriangleComputeShader);
}

} // namespace vex::dx12