#include "DX12DescriptorPool.h"

#include <DX12/DX12Sampler.h>

namespace vex::dx12
{

DX12DescriptorPool::DX12DescriptorPool(ComPtr<DX12Device>& device)
    // TODO(https://trello.com/c/uGignSlW): allow resizing of the GPU heap, currently disallowed due to synchro issues.
    // The sampler heap however cannot be larger than GMaxBindlessSamplerCount.
    : device{ device }
    , gpuHeap(device, GDefaultDescriptorPoolSize, "ResourceDescriptorHeap")
    , samplerHeap(device, GMaxBindlessSamplerCount, "SamplerDescriptorHeap")
    , nullHeap(device, 1, "NullResourceDescriptorHeap")
    , samplerNullHeap(device, 1, "NullSamplerDescriptorHeap")
{
    samplerAllocator = {
        .generations = std::vector<u8>(GMaxBindlessSamplerCount),
        .handles = FreeListAllocator(GMaxBindlessSamplerCount),
    };
    // Fill in the null heap with a null SRV.
    static constexpr D3D12_SHADER_RESOURCE_VIEW_DESC NullDesc = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D =
        {
            .MipLevels = 1,
        },
    };
    device->CreateShaderResourceView(nullptr, &NullDesc, GetNullResourceDescriptor());

    static constexpr D3D12_SAMPLER_DESC NullSamplerDesc = {
        .Filter = D3D12_FILTER_MIN_MAG_MIP_POINT,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    };
    device->CreateSampler(&NullSamplerDesc, GetNullSamplerDescriptor());
}

void DX12DescriptorPool::CopyNullDescriptor(DescriptorType descriptorType, u32 slotIndex)
{
    if (descriptorType == DescriptorType::Resource)
    {
        device->CopyDescriptorsSimple(1,
                                      gpuHeap.GetCPUDescriptorHandle(slotIndex),
                                      GetNullResourceDescriptor(),
                                      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    else // if (descriptorType == DescriptorType::Sampler)
    {
        device->CopyDescriptorsSimple(1,
                                      samplerHeap.GetCPUDescriptorHandle(slotIndex),
                                      GetNullSamplerDescriptor(),
                                      D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }
}

BindlessHandle DX12DescriptorPool::CreateBindlessSampler(const BindlessTextureSampler& textureSampler)
{
    const BindlessHandle bindlessHandle = AllocateStaticDescriptor(DescriptorType::Sampler);
    const D3D12_SAMPLER_DESC samplerDesc = GraphicsPipeline::GetDX12SamplerDescFromBindlessTextureSampler(textureSampler);
    device->CreateSampler(&samplerDesc, samplerHeap.GetCPUDescriptorHandle(bindlessHandle.GetIndex()));
    return bindlessHandle;
}

void DX12DescriptorPool::FreeBindlessSampler(BindlessHandle handle)
{
    FreeStaticDescriptor(DescriptorType::Sampler, handle);
}

void DX12DescriptorPool::CopyDescriptor(BindlessHandle handle, CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor)
{
    VEX_ASSERT(IsValid(handle), "Invalid handle passed to DX12 Descriptor Pool.");
    device->CopyDescriptorsSimple(1, GetCPUDescriptor(handle), descriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12DescriptorPool::GetCPUDescriptor(BindlessHandle handle)
{
    VEX_ASSERT(IsValid(handle), "Invalid handle passed to DX12 Descriptor Pool.");
    return gpuHeap.GetCPUDescriptorHandle(handle.GetIndex());
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12DescriptorPool::GetNullResourceDescriptor()
{
    return nullHeap.GetCPUDescriptorHandle(0);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12DescriptorPool::GetNullSamplerDescriptor()
{
    return samplerNullHeap.GetCPUDescriptorHandle(0);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DX12DescriptorPool::GetGPUDescriptor(BindlessHandle handle)
{
    VEX_ASSERT(IsValid(handle), "Invalid handle passed to DX12 Descriptor Pool.");
    return gpuHeap.GetGPUDescriptorHandle(handle.GetIndex());
}

} // namespace vex::dx12