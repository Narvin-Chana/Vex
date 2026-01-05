#include "DX12Buffer.h"

#include <Vex/Bindings.h>
#include <Vex/Buffer.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Debug.h>
#include <Vex/Platform/Platform.h>
#include <Vex/RHIImpl/RHIAllocator.h>
#include <Vex/Utility/ByteUtils.h>

#include <DX12/DX12FeatureChecker.h>
#include <DX12/RHI/DX12DescriptorPool.h>

namespace vex::dx12
{

DX12Buffer::DX12Buffer(ComPtr<DX12Device>& device, RHIAllocator& allocator, const BufferDesc& desc)
    : RHIBufferBase(allocator, desc)
    , device(device)
{
    u64 size = desc.byteSize;

    // Size of constant buffers need to be multiples of 256. User won't know its bigger so it shouldn't be an issue.
    if (desc.usage & BufferUsage::UniformBuffer)
    {
        size = AlignUp<u64>(desc.byteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    }

    CD3DX12_RESOURCE_DESC1 bufferDesc = CD3DX12_RESOURCE_DESC1::Buffer(size,
                                                                       (desc.usage & BufferUsage::ReadWriteBuffer)
                                                                           ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                                                                           : D3D12_RESOURCE_FLAG_NONE);
    if (VEX_USE_CUSTOM_ALLOCATOR_BUFFERS &&
        static_cast<DX12FeatureChecker*>(GPhysicalDevice->featureChecker.get())->SupportsTightAlignment())
    {
        bufferDesc.Flags |= D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT;
    }

    if (desc.usage & BufferUsage::AccelerationStructure)
    {
        bufferDesc.Flags |= D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE;
        // TODO(https://trello.com/c/rLevCOvT): Decide if this should be moved up into the Vex layer or not!
        // This depends on how Vulkan implements HWRT.
        VEX_ASSERT(bufferDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                   "Acceleration Structure buffer usage flag also requires the UnorderedAccess flag!");
    }

    CD3DX12_HEAP_PROPERTIES heapProps;

    if (desc.memoryLocality == ResourceMemoryLocality::CPURead)
    {
        heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    }
    else if (desc.memoryLocality == ResourceMemoryLocality::CPUWrite)
    {
        heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    }
    else
    {
        heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    }

#if VEX_USE_CUSTOM_ALLOCATOR_BUFFERS
    allocation = allocator.AllocateResource(buffer, bufferDesc, desc.memoryLocality);
#else
    chk << device->CreateCommittedResource3(&heapProps,
                                            D3D12_HEAP_FLAG_NONE,
                                            &bufferDesc,
                                            D3D12_BARRIER_LAYOUT_UNDEFINED,
                                            nullptr,
                                            nullptr,
                                            0,
                                            nullptr,
                                            IID_PPV_ARGS(&buffer));
#endif

#if !VEX_SHIPPING
    chk << buffer->SetName(StringToWString(std::format("Buffer: {}", desc.name)).data());
#endif
}

Span<byte> DX12Buffer::Map()
{
    void* ptr = nullptr;
    D3D12_RANGE range{
        .Begin = 0,
        .End = desc.byteSize,
    };
    chk << buffer->Map(0, &range, &ptr);
    return { static_cast<byte*>(ptr), desc.byteSize };
}

void DX12Buffer::Unmap()
{
    // CPURead mapped buffers have no real purpose in being "unmapped" as they are always available.
    // TODO(https://trello.com/c/lsqpXupB): We have to eventually rework mapping to be done on creation instead of when
    // needed, but until then we early return to avoid validation layer warnings.
    if (desc.memoryLocality == ResourceMemoryLocality::CPURead)
    {
        return;
    }

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

void DX12Buffer::AllocateBindlessHandle(RHIDescriptorPool& descriptorPool,
                                        BindlessHandle handle,
                                        const BufferViewDesc& viewDesc)
{
    BufferBindingUsage usage = viewDesc.usage;
    const bool isCBV = usage == BufferBindingUsage::ConstantBuffer;
    const bool isSRV = usage == BufferBindingUsage::StructuredBuffer || usage == BufferBindingUsage::ByteAddressBuffer;
    const bool isUAV =
        usage == BufferBindingUsage::RWStructuredBuffer || usage == BufferBindingUsage::RWByteAddressBuffer;
    const bool isAccelerationStructure = desc.usage & BufferUsage::AccelerationStructure;

    VEX_ASSERT(isSRV || isUAV || isCBV || isAccelerationStructure,
               "The bindless view requested for buffer '{}' must be either of type SRV, CBV, UAV or the underlying "
               "buffer should be an Acceleration Structure.",
               desc.name);

    auto cpuHandle = descriptorPool.GetCPUDescriptor(handle);

    if (isCBV)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = buffer->GetGPUVirtualAddress() +
                                 AlignUp<u64>(viewDesc.offsetByteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        cbvDesc.SizeInBytes = AlignUp<u64>(viewDesc.rangeByteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
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
            srvDesc.Buffer.FirstElement = viewDesc.GetFirstElement();
            srvDesc.Buffer.NumElements = viewDesc.GetElementCount();
            srvDesc.Buffer.StructureByteStride = viewDesc.strideByteSize;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            break;
        case BufferBindingUsage::ByteAddressBuffer:
            srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = viewDesc.GetFirstElement();
            srvDesc.Buffer.NumElements = viewDesc.GetElementCount();
            srvDesc.Buffer.StructureByteStride = 0;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            break;
        default:
            break;
        }

        device->CreateShaderResourceView(buffer.Get(), &srvDesc, cpuHandle);
    }
    else if (isAccelerationStructure)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
            .Format = DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .RaytracingAccelerationStructure = { .Location = GetGPUVirtualAddress() },
        };
        device->CreateShaderResourceView(nullptr, &srvDesc, cpuHandle);
    }
    else // if (isUAVView)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};

        switch (usage)
        {
        case BufferBindingUsage::RWStructuredBuffer:
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = viewDesc.GetFirstElement();
            uavDesc.Buffer.NumElements = viewDesc.GetElementCount();
            uavDesc.Buffer.StructureByteStride = viewDesc.strideByteSize;
            uavDesc.Buffer.CounterOffsetInBytes = 0;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
            break;
        case BufferBindingUsage::RWByteAddressBuffer:
            uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = viewDesc.GetFirstElement();
            uavDesc.Buffer.NumElements = viewDesc.GetElementCount();
            uavDesc.Buffer.StructureByteStride = 0;
            uavDesc.Buffer.CounterOffsetInBytes = 0;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            break;
        default:
            break;
        }

        device->CreateUnorderedAccessView(buffer.Get(), nullptr, &uavDesc, cpuHandle);
    }
}

} // namespace vex::dx12
