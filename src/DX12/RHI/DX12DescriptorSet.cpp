#include "DX12DescriptorSet.h"

#include "DX12/DX12Headers.h"

namespace vex::dx12
{

DX12BindlessDescriptorSet::DX12BindlessDescriptorSet(ComPtr<DX12Device>& device)
    // TODO: allow resizing, currently disallowed
    : device{ device }
    , gpuHeap(device, GDefaultDescriptorPoolSize)
    , nullHeap(device, 1)
{
    // Fill in the null heap with a null SRV.
    D3D12_SHADER_RESOURCE_VIEW_DESC nullDesc = {};
    nullDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    nullDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    nullDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    nullDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(nullptr, &nullDesc, GetNullDescriptor());
}

void DX12BindlessDescriptorSet::CopyNullDescriptor(u32 slotIndex)
{
    device->CopyDescriptorsSimple(1,
                                  gpuHeap.GetCPUDescriptorHandle(slotIndex),
                                  GetNullDescriptor(),
                                  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DX12BindlessDescriptorSet::CopyDescriptor(BindlessHandle handle, CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor)
{
    VEX_ASSERT(IsValid(handle), "Invalid handle passed to DX12 Descriptor Pool.");
    device->CopyDescriptorsSimple(1, GetCPUDescriptor(handle), descriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12BindlessDescriptorSet::GetCPUDescriptor(BindlessHandle handle)
{
    VEX_ASSERT(IsValid(handle), "Invalid handle passed to DX12 Descriptor Pool.");
    return gpuHeap.GetCPUDescriptorHandle(handle.GetIndex());
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12BindlessDescriptorSet::GetNullDescriptor()
{
    return nullHeap.GetCPUDescriptorHandle(0);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DX12BindlessDescriptorSet::GetGPUDescriptor(BindlessHandle handle)
{
    VEX_ASSERT(IsValid(handle), "Invalid handle passed to DX12 Descriptor Pool.");
    return gpuHeap.GetGPUDescriptorHandle(handle.GetIndex());
}

} // namespace vex::dx12