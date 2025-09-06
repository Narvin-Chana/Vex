#include "DX12Buffer.h"

#include <Vex/Bindings.h>
#include <Vex/Buffer.h>
#include <Vex/ByteUtils.h>
#include <Vex/Debug.h>
#include <Vex/Logger.h>
#include <Vex/Platform/Platform.h>
#include <Vex/RHIImpl/RHIAllocator.h>

namespace vex::dx12
{

DX12Buffer::DX12Buffer(ComPtr<DX12Device>& device, RHIAllocator& allocator, const BufferDescription& desc)
    : RHIBufferBase(allocator, desc)
    , device(device)
{
    u64 size = desc.byteSize;

    // Size of constant buffers need to be multiples of 256. User won't know its bigger so it shouldn't be an issue.
    if (desc.usage & BufferUsage::UniformBuffer)
    {
        size = AlignUp<u64>(desc.byteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    }

    const CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
        size,
        (desc.usage & BufferUsage::ReadWriteBuffer) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                                                    : D3D12_RESOURCE_FLAG_NONE);
    CD3DX12_HEAP_PROPERTIES heapProps;
    D3D12_RESOURCE_STATES dxInitialState = D3D12_RESOURCE_STATE_COMMON;

    if (desc.memoryLocality == ResourceMemoryLocality::CPURead)
    {
        heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        dxInitialState = D3D12_RESOURCE_STATE_COPY_DEST;
        currentState = RHIBufferState::CopyDest;
    }
    else if (desc.memoryLocality == ResourceMemoryLocality::CPUWrite)
    {
        heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        dxInitialState = D3D12_RESOURCE_STATE_GENERIC_READ;
        currentState = RHIBufferState::ShaderResource;
    }
    else
    {
        heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        // Default state is conserved.
    }

#if VEX_USE_CUSTOM_ALLOCATOR_BUFFERS
    allocation = allocator.AllocateResource(buffer, bufferDesc, desc.memoryLocality, dxInitialState);
#else
    chk << device->CreateCommittedResource(&heapProps,
                                           D3D12_HEAP_FLAG_NONE,
                                           &bufferDesc,
                                           dxInitialState,
                                           nullptr,
                                           IID_PPV_ARGS(&buffer));
#endif

#if !VEX_SHIPPING
    chk << buffer->SetName(StringToWString(desc.name).data());
#endif
}

std::span<byte> DX12Buffer::Map()
{
    void* ptr;
    D3D12_RANGE range{
        .Begin = 0,
        .End = desc.byteSize,
    };
    chk << buffer->Map(0, &range, &ptr);
    return { static_cast<byte*>(ptr), desc.byteSize };
}

void DX12Buffer::Unmap()
{
    D3D12_RANGE range{
        .Begin = 0,
        .End = desc.byteSize,
    };
    buffer->Unmap(0, &range);
}

D3D12_VERTEX_BUFFER_VIEW DX12Buffer::GetVertexBufferView(const BufferBinding& binding) const
{
    return D3D12_VERTEX_BUFFER_VIEW{
        .BufferLocation = GetGPUVirtualAddress() + binding.offsetByteSize.value_or(0),
        .SizeInBytes = static_cast<u32>(desc.byteSize),
        .StrideInBytes = *binding.strideByteSize,
    };
}

D3D12_INDEX_BUFFER_VIEW DX12Buffer::GetIndexBufferView(const BufferBinding& binding) const
{
    DXGI_FORMAT format;
    switch (*binding.strideByteSize)
    {
    case 2:
        format = DXGI_FORMAT_R16_UINT;
        break;
    case 4:
        format = DXGI_FORMAT_R32_UINT;
        break;
    default:
        VEX_LOG(Fatal,
                "DX12RHI: DX12Buffer's IndexBufferView cannot be created with a stride different than 2 or 4 bytes.");
    }
    return D3D12_INDEX_BUFFER_VIEW{
        .BufferLocation = GetGPUVirtualAddress() + binding.offsetByteSize.value_or(0),
        .SizeInBytes = static_cast<u32>(desc.byteSize),
        .Format = format,
    };
}

BindlessHandle DX12Buffer::GetOrCreateBindlessView(BufferBindingUsage usage,
                                                   std::optional<u32> strideByteSize,
                                                   DX12DescriptorPool& descriptorPool)
{
    bool isCBV = usage == BufferBindingUsage::ConstantBuffer;
    bool isSRV = usage == BufferBindingUsage::StructuredBuffer || usage == BufferBindingUsage::ByteAddressBuffer;
    bool isUAV = usage == BufferBindingUsage::RWStructuredBuffer || usage == BufferBindingUsage::RWByteAddressBuffer;

    VEX_ASSERT(isSRV || isUAV || isCBV,
               "The bindless view requested for buffer '{}' must be either of type SRV, CBV or UAV.",
               desc.name);

    // Check cache first
    BufferViewCacheKey cacheKey{ usage, strideByteSize };
    if (auto it = viewCache.find(cacheKey); it != viewCache.end() && descriptorPool.IsValid(it->second))
    {
        return it->second;
    }

    BindlessHandle handle = descriptorPool.AllocateStaticDescriptor();
    auto cpuHandle = descriptorPool.GetCPUDescriptor(handle);

    if (isCBV)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = buffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = AlignUp<u32>(desc.byteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        device->CreateConstantBufferView(&cbvDesc, cpuHandle);
    }
    else if (isSRV)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

        switch (usage)
        {
        case BufferBindingUsage::StructuredBuffer:
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.NumElements = desc.byteSize / *strideByteSize;
            srvDesc.Buffer.StructureByteStride = *strideByteSize;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            break;
        case BufferBindingUsage::ByteAddressBuffer:
            srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.NumElements = desc.byteSize / 4; // R32 elements (4 bytes each)
            srvDesc.Buffer.StructureByteStride = 0;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            break;
        default:
            break;
        }

        device->CreateShaderResourceView(buffer.Get(), &srvDesc, cpuHandle);
    }
    else // if (isUAVView)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};

        switch (usage)
        {
        case BufferBindingUsage::RWStructuredBuffer:
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = 0;
            uavDesc.Buffer.NumElements = desc.byteSize / *strideByteSize;
            uavDesc.Buffer.StructureByteStride = *strideByteSize;
            uavDesc.Buffer.CounterOffsetInBytes = 0;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
            break;
        case BufferBindingUsage::RWByteAddressBuffer:
            uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = 0;
            uavDesc.Buffer.NumElements = desc.byteSize / 4; // R32 elements (4 bytes each)
            uavDesc.Buffer.StructureByteStride = 0;
            uavDesc.Buffer.CounterOffsetInBytes = 0;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            break;
        default:
            break;
        }

        device->CreateUnorderedAccessView(buffer.Get(), nullptr, &uavDesc, cpuHandle);
    }

    viewCache[cacheKey] = handle;
    return handle;
}

void DX12Buffer::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    for (const auto& [cacheKey, bindlessHandle] : viewCache)
    {
        if (bindlessHandle != GInvalidBindlessHandle)
        {
            descriptorPool.FreeStaticDescriptor(bindlessHandle);
        }
    }
    viewCache.clear();
}

void DX12Buffer::FreeAllocation(RHIAllocator& allocator)
{
#if VEX_USE_CUSTOM_ALLOCATOR_BUFFERS
    allocator.FreeResource(allocation);
#endif
}

} // namespace vex::dx12
