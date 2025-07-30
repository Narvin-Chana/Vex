#pragma once

#include <unordered_map>

#include <Vex/RHI/RHIBuffer.h>

#include <DX12/DX12DescriptorHeap.h>
#include <DX12/DX12DescriptorPool.h>
#include <DX12/DX12Headers.h>

namespace vex
{
struct BufferDescription;
}
namespace vex::dx12
{

class DX12Buffer : public RHIBuffer
{
public:
    DX12Buffer(ComPtr<DX12Device>& device, const BufferDescription& desc);

    virtual std::span<u8> Map() override;
    virtual void Unmap() override;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;

    BindlessHandle GetOrCreateBindlessView(BufferUsage::Type usage, DX12DescriptorPool& descriptorPool);

    ID3D12Resource* GetRawBuffer()
    {
        return buffer.Get();
    }

private:
    virtual UniqueHandle<RHIBuffer> CreateStagingBuffer() override;

    ComPtr<DX12Device> device;

    ComPtr<ID3D12Resource> buffer;

    std::unordered_map<BufferUsage::Type, BindlessHandle> viewCache;
};

} // namespace vex::dx12
