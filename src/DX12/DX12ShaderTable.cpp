#include "DX12ShaderTable.h"

#include <Vex/Platform/Debug.h>
#include <Vex/Utility/ByteUtils.h>

namespace vex::dx12
{

DX12ShaderTable::DX12ShaderTable(ComPtr<DX12Device>& device,
                                 std::string_view name,
                                 RHIAllocator& allocator,
                                 Span<void*> shaderIdentifiers)
{
    if (shaderIdentifiers.empty())
    {
        VEX_LOG(Fatal, "Cannot create a shader table from an empty identifiers list.");
        return;
    }

    // We can directly use the identifier size as shader record size.
    // This is incorrect IF you use local root signatures in RT shader tables, which Vex does not support due to
    // cross-compatibility with Vulkan.
    static constexpr u32 ShaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

    // Ensures proper alignment - shader records must be aligned to D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT (32
    // bytes)
    static constexpr u64 AlignedRecordStride =
        AlignUp<u64>(ShaderRecordSize, std::max(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
    recordStride = AlignedRecordStride;

    BufferDesc desc = BufferDesc::CreateStagingBufferDesc(std::string(name),
                                                          recordStride * shaderIdentifiers.size(),
                                                          BufferUsage::GenericBuffer | BufferUsage::ShaderTable);
    buffer = RHIBuffer(device, allocator, std::move(desc));

    Span<byte> mappedData = buffer->GetMappedData();

    // Write each shader identifier.
    for (u64 i = 0; i < shaderIdentifiers.size(); ++i)
    {
        u64 offset = i * recordStride;

        // Copy the shader identifier.
        memcpy(mappedData.data() + offset, shaderIdentifiers[i], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Zero out any padding.
        if (recordStride > D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES)
        {
            memset(mappedData.data() + offset + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
                   0,
                   recordStride - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        }
    }
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE DX12ShaderTable::GetVirtualAddressRangeAndStride(u32 offset) const
{
    VEX_ASSERT(recordStride != 0,
               "Cannot obtain D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE from a zero-stride ShaderTable.");

    return D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{
        .StartAddress = buffer->GetGPUVirtualAddress() + offset * recordStride,
        .SizeInBytes = buffer->GetDesc().byteSize - offset * recordStride,
        .StrideInBytes = recordStride,
    };
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE DX12ShaderTable::GetVirtualAddressRange(u32 offset) const
{
    VEX_ASSERT(recordStride != 0,
               "Cannot obtain D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE from a zero-stride ShaderTable.");

    return D3D12_GPU_VIRTUAL_ADDRESS_RANGE{
        .StartAddress = buffer->GetGPUVirtualAddress() + offset * recordStride,
        .SizeInBytes = recordStride,
    };
}

} // namespace vex::dx12