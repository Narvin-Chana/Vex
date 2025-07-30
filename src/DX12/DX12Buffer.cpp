#include "DX12Buffer.h"

#include <Vex/Buffer.h>
#include <Vex/Debug.h>
#include <Vex/Logger.h>
#include <Vex/Platform/Platform.h>

namespace vex::dx12
{

DX12Buffer::DX12Buffer(ComPtr<DX12Device>& device, const BufferDescription& desc)
    : RHIBuffer(desc)
    , device(device)
{
    const CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
        desc.byteSize,
        (desc.usage & BufferUsage::ShaderReadWrite) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                                                    : D3D12_RESOURCE_FLAG_NONE);
    CD3DX12_HEAP_PROPERTIES heapProps;
    D3D12_RESOURCE_STATES dxInitialState = D3D12_RESOURCE_STATE_COMMON;

    if (desc.usage & BufferUsage::CPUVisible)
    {
        heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        dxInitialState = D3D12_RESOURCE_STATE_COPY_DEST;
        currentState = RHIBufferState::CopyDest;
    }
    else if (desc.usage & BufferUsage::CPUWrite)
    {
        heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        dxInitialState = D3D12_RESOURCE_STATE_GENERIC_READ;
        currentState = RHIBufferState::ShaderResource;
    }
    else if (desc.usage != BufferUsage::None)
    {
        heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        // Default state is conserved.
    }
    else
    {
        VEX_LOG(Fatal, "Unsupported buffer description, usage does not map to DX12.");
    }

    chk << device->CreateCommittedResource(&heapProps,
                                           D3D12_HEAP_FLAG_NONE,
                                           &bufferDesc,
                                           dxInitialState,
                                           nullptr,
                                           IID_PPV_ARGS(&buffer));

#if !VEX_SHIPPING
    chk << buffer->SetName(StringToWString(desc.name).data());
#endif
}

UniqueHandle<RHIBuffer> DX12Buffer::CreateStagingBuffer()
{
    return MakeUnique<DX12Buffer>(device,
                                  BufferDescription{
                                      .name = desc.name + "_StagingBuffer",
                                      .byteSize = desc.byteSize,
                                      .usage = BufferUsage::CPUWrite,
                                  });
}

std::span<u8> DX12Buffer::Map()
{
    void* ptr;
    D3D12_RANGE range{
        .Begin = 0,
        .End = desc.byteSize,
    };
    chk << buffer->Map(0, &range, &ptr);
    return { static_cast<u8*>(ptr), desc.byteSize };
}

void DX12Buffer::Unmap()
{
    D3D12_RANGE range{
        .Begin = 0,
        .End = desc.byteSize,
    };
    buffer->Unmap(0, &range);
}

void DX12Buffer::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    for (BindlessHandle bindlessHandle : viewCache | std::views::values)
    {
        if (bindlessHandle != GInvalidBindlessHandle)
        {
            reinterpret_cast<DX12DescriptorPool&>(descriptorPool).FreeStaticDescriptor(bindlessHandle);
        }
    }
    viewCache.clear();
}

BindlessHandle DX12Buffer::GetOrCreateBindlessView(BufferUsage::Type usage, DX12DescriptorPool& descriptorPool)
{
    // Usage is the exact correct usage (no longer flags), so == is valid here.
    bool isSRVView = (usage == BufferUsage::ShaderRead) && (desc.usage & BufferUsage::ShaderRead);
    bool isUAVView = (usage == BufferUsage::ShaderReadWrite) && (desc.usage & BufferUsage::ShaderReadWrite);

    VEX_ASSERT(
        isSRVView || isUAVView,
        "The bindless view requested for buffer '{}' must be either of type SRV or UAV (ShaderRead or ShaderReadWrite).",
        desc.name);

    // Check cache first
    if (auto it = viewCache.find(usage); it != viewCache.end() && descriptorPool.IsValid(it->second))
    {
        return it->second;
    }

    BindlessHandle handle = descriptorPool.AllocateStaticDescriptor();

    if (isSRVView)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

        if (desc.IsStructured())
        {
            // As a StructuredBuffer
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.NumElements = desc.byteSize / desc.stride;
            srvDesc.Buffer.StructureByteStride = desc.stride;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        }
        else
        {
            // As a raw ByteAddressBuffer
            srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.NumElements = desc.byteSize / 4; // R32 elements (4 bytes each)
            srvDesc.Buffer.StructureByteStride = 0;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        }

        auto cpuHandle = descriptorPool.GetCPUDescriptor(handle);
        device->CreateShaderResourceView(buffer.Get(), &srvDesc, cpuHandle);
    }
    else // if (isUAVView)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};

        if (desc.IsStructured())
        {
            // As a RWStructuredBuffer
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = 0;
            uavDesc.Buffer.NumElements = desc.byteSize / desc.stride;
            uavDesc.Buffer.StructureByteStride = desc.stride;
            uavDesc.Buffer.CounterOffsetInBytes = 0;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        }
        else
        {
            // As a raw RWByteAddressBuffer
            uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = 0;
            uavDesc.Buffer.NumElements = desc.byteSize / 4; // R32 elements (4 bytes each)
            uavDesc.Buffer.StructureByteStride = 0;
            uavDesc.Buffer.CounterOffsetInBytes = 0;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        }

        auto cpuHandle = descriptorPool.GetCPUDescriptor(handle);
        device->CreateUnorderedAccessView(buffer.Get(), nullptr, &uavDesc, cpuHandle);
    }

    viewCache[usage] = handle;
    return handle;
}

} // namespace vex::dx12
