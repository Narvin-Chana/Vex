#include "DX12ShaderTable.h"

#include <Vex/Utility/ByteUtils.h>
#include <Vex/Debug.h>

namespace vex::dx12
{

DX12ShaderTable::DX12ShaderTable(ComPtr<DX12Device>& device,
                                 RHIAllocator& allocator,
                                 const BufferDesc& desc,
                                 std::span<void*> shaderIdentifiers,
                                 u64 shaderIdentifierSize,
                                 u64 recordStride)
    : buffer(device, allocator, desc)
{
    if (shaderIdentifiers.empty())
    {
        VEX_LOG(Fatal, "Cannot create a shader table from an empty identifiers list.");
        return;
    }

    if (desc.memoryLocality != ResourceMemoryLocality::CPUWrite)
    {
        VEX_LOG(Fatal, "Cannot create a shader table with a non-CPUWrite buffer.");
        return;
    }

    // Ensures proper alignment - shader records must be aligned to D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT (32
    // bytes)
    u64 alignedRecordStride = AlignUp<u64>(recordStride, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
    shaderTableStride = alignedRecordStride;

    std::span<byte> mappedData = buffer.Map();

    // Write each shader identifier.
    for (u64 i = 0; i < shaderIdentifiers.size(); ++i)
    {
        u64 offset = i * alignedRecordStride;

        // Copy the shader identifier.
        memcpy(mappedData.data() + offset, shaderIdentifiers[i], shaderIdentifierSize);

        // Zero out any padding.
        if (alignedRecordStride > shaderIdentifierSize)
        {
            memset(mappedData.data() + offset + shaderIdentifierSize, 0, alignedRecordStride - shaderIdentifierSize);
        }
    }

    buffer.Unmap();
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE DX12ShaderTable::GetVirtualAddressRangeAndStride() const
{
    VEX_ASSERT(shaderTableStride != 0,
               "Cannot obtain D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE from a zero-stride ShaderTable.");

    return D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{
        .StartAddress = buffer.GetGPUVirtualAddress(),
        .SizeInBytes = buffer.GetDesc().byteSize,
        .StrideInBytes = shaderTableStride,
    };
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE DX12ShaderTable::GetVirtualAddressRange() const
{
    return D3D12_GPU_VIRTUAL_ADDRESS_RANGE{
        .StartAddress = buffer.GetGPUVirtualAddress(),
        .SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
    };
}

} // namespace vex::dx12