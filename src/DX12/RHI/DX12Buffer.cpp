#include "DX12Buffer.h"

#include <Vex/Bindings.h>
#include <Vex/Buffer.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Debug.h>
#include <Vex/Platform/Platform.h>
#include <Vex/RHIImpl/RHIAllocator.h>
#include <Vex/Utility/ByteUtils.h>
#include <Vex/Utility/Validation.h>

#include <DX12/RHI/DX12DescriptorPool.h>

namespace vex::dx12
{

DX12Buffer::DX12Buffer(ComPtr<DX12Device>& device, RHIAllocator& allocator, const BufferDesc& desc)
    : RHIBufferBase(allocator, desc)
    , device(device)
{
    u64 size = desc.byteSize;
    u64 forcedAlignment = 0;

    if (desc.usage & BufferUsage::UniformBuffer)
    {
        // Constant buffers need to be aligned to 256.
        forcedAlignment = std::max<u64>(forcedAlignment, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        // Force size to 256 alignment, in order to avoid issues later on when creating CBVs (CBVs must be 256 bytes
        // aligned).
        size = AlignUp<u64>(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    }

    CD3DX12_RESOURCE_DESC1 bufferDesc = CD3DX12_RESOURCE_DESC1::Buffer(size,
                                                                       (desc.usage & BufferUsage::ReadWriteBuffer)
                                                                           ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                                                                           : D3D12_RESOURCE_FLAG_NONE);
    if (VEX_USE_CUSTOM_ALLOCATOR_BUFFERS && GPhysicalDevice->SupportsTightAlignment())
    {
        bufferDesc.Flags |= D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT;
    }

    if (desc.usage & BufferUsage::AccelerationStructure)
    {
        bufferDesc.Flags |= D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE;
        // TODO(https://trello.com/c/rLevCOvT): Decide if this should be moved up into the Vex layer or not!
        // This depends on how Vulkan implements HWRT.
        VEX_ASSERT(desc.usage & BufferUsage::ReadWriteBuffer,
                   "Acceleration Structure usage requires the ReadWriteBuffer usage flag.");
        VEX_ASSERT(bufferDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                   "Acceleration Structure buffer usage flag also requires the UnorderedAccess flag!");

        // RT acceleration structures have a higher alignment requirement, for some reason GetResourceAllocationInfo3,
        // does not return the correct alignment.
        forcedAlignment = std::max<u64>(forcedAlignment, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
    }

    if (desc.usage & BufferUsage::ScratchBuffer)
    {
        // RT scratch buffers have a higher alignment requirement, for some reason GetResourceAllocationInfo3,
        // does not return the correct alignment.
        forcedAlignment = std::max<u64>(forcedAlignment, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
    }

    if (desc.usage & BufferUsage::ShaderTable)
    {
        // RT Shader Tables should follow D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT.
        forcedAlignment = std::max<u64>(forcedAlignment, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
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
    allocation = allocator.AllocateResource(buffer, bufferDesc, desc.memoryLocality, forcedAlignment);
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

    if (IsMappable())
    {
        void* ptr = nullptr;
        D3D12_RANGE range{
            .Begin = 0,
            .End = desc.byteSize,
        };
        chk << buffer->Map(0, &range, &ptr);
        mappedData = std::span<byte>{ static_cast<byte*>(ptr), range.End };
        // The buffer will remain mapped until destruction, where DX12 cleanup will automatically unmap.
        // Meaning we don't have to ever call Unmap.
    }

#if !VEX_SHIPPING
    chk << buffer->SetName(StringToWString(std::format("Buffer: {}", desc.name)).data());
#endif
}

D3D12_VERTEX_BUFFER_VIEW DX12Buffer::GetVertexBufferView(const BufferBinding& binding) const
{
    return D3D12_VERTEX_BUFFER_VIEW{
        .BufferLocation = GetGPUVirtualAddress() + binding.offsetByteSize.value_or(0),
        .SizeInBytes = static_cast<u32>(binding.rangeByteSize.value_or(desc.byteSize)),
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
        .SizeInBytes = static_cast<u32>(binding.rangeByteSize.value_or(desc.byteSize)),
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
        VEX_CHECK(IsAligned<u64>(viewDesc.offsetByteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT),
                  "DX12 requires that constant buffer locations be aligned to 256. If you want more precise offsets, "
                  "use a raw ByteAddressBuffer to access your resource!");
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = buffer->GetGPUVirtualAddress() + viewDesc.offsetByteSize;
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
