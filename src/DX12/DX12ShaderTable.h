#pragma once

#include <string_view>

#include <Vex/Containers/Span.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/Utility/MaybeUninitialized.h>

namespace vex::dx12
{

struct DX12ShaderTable
{
    DX12ShaderTable(ComPtr<DX12Device>& device,
                    std::string_view name,
                    RHIAllocator& allocator,
                    Span<void*> shaderIdentifiers);

    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE GetVirtualAddressRangeAndStride(u32 offset) const;
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE GetVirtualAddressRange(u32 offset) const;

    MaybeUninitialized<RHIBuffer> buffer;
    u32 recordStride = 0;
};

} // namespace vex::dx12