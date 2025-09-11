#pragma once

#include <RHI/RHIDescriptorPool.h>

#include <DX12/DX12DescriptorHeap.h>
#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12DescriptorPool final : public RHIDescriptorPoolBase
{
public:
    DX12DescriptorPool(ComPtr<DX12Device>& device);
    virtual void CopyNullBindlessDescriptor(u32 slotIndex) override;

    void CopyDescriptor(BindlessHandle handle, CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor);
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptor(BindlessHandle handle);
    CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptor(BindlessHandle handle);

    ComPtr<ID3D12DescriptorHeap>& GetNativeDescriptorHeap()
    {
        return gpuHeap.GetNativeDescriptorHeap();
    }

private:
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetNullDescriptor();

    ComPtr<DX12Device> device;

    DX12DescriptorHeap<DX12HeapType::CBV_SRV_UAV, HeapFlags::SHADER_VISIBLE> gpuHeap;

    // Used to store a null descriptor, useful for avoiding invalid texture usage (and avoiding gpu hangs) if a shader
    // ever tries to access an invalid resource.
    DX12DescriptorHeap<DX12HeapType::CBV_SRV_UAV, HeapFlags::NONE> nullHeap;

    friend class DX12CommandList;
};
} // namespace vex::dx12