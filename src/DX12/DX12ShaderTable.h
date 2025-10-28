#pragma once

#include <Vex/RHIImpl/RHIBuffer.h>

namespace vex::dx12
{

struct DX12ShaderTable
{
    DX12ShaderTable(ComPtr<DX12Device>& device,
                    RHIAllocator& allocator,
                    const BufferDesc& desc,
                    std::span<void*> shaderIdentifiers,
                    u64 shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
                    u64 recordStride = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE GetVirtualAddressRangeAndStride() const;
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE GetVirtualAddressRange() const;

    RHIBuffer buffer;
    u32 shaderTableStride = 0;
};

} // namespace vex::dx12