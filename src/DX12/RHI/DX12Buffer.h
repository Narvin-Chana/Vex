#pragma once

#include <unordered_map>

#include <RHI/RHIBuffer.h>

#include <DX12/DX12Headers.h>

namespace vex
{
struct BufferDesc;
struct BufferBinding;
} // namespace vex

namespace vex::dx12
{

class DX12Buffer final : public RHIBufferBase
{
public:
    DX12Buffer(ComPtr<DX12Device>& device, RHIAllocator& allocator, const BufferDesc& desc);

    virtual void AllocateBindlessHandle(RHIDescriptorPool& descriptorPool,
                                        BindlessHandle handle,
                                        const BufferViewDesc& viewDesc) override;

    virtual std::span<byte> Map() override;
    virtual void Unmap() override;

    ID3D12Resource* GetRawBuffer()
    {
        return buffer.Get();
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const
    {
        return buffer->GetGPUVirtualAddress();
    }

    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView(const BufferBinding& binding) const;
    D3D12_INDEX_BUFFER_VIEW GetIndexBufferView(const BufferBinding& binding) const;

private:
    ComPtr<DX12Device> device;

    ComPtr<ID3D12Resource> buffer;
};

} // namespace vex::dx12
