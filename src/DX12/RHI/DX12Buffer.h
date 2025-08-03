#pragma once

#include <unordered_map>

#include <RHI/RHIBuffer.h>

#include <DX12/DX12DescriptorHeap.h>
#include <DX12/DX12Headers.h>
#include <DX12/RHI/DX12DescriptorPool.h>

namespace vex
{
struct BufferDescription;
}

namespace vex::dx12
{

struct BufferViewCacheKey
{
    BufferBindingUsage usage;
    u32 stride;

    bool operator==(const BufferViewCacheKey& other) const = default;
};

} // namespace vex::dx12

// clang-format off

VEX_MAKE_HASHABLE(vex::dx12::BufferViewCacheKey,
    VEX_HASH_COMBINE(seed, obj.usage);
    VEX_HASH_COMBINE(seed, obj.stride);
);

// clang-format on

namespace vex::dx12
{

class DX12Buffer final : public RHIBufferInterface
{
public:
    DX12Buffer(ComPtr<DX12Device>& device, const BufferDescription& desc);

    virtual std::span<u8> Map() override;
    virtual void Unmap() override;

    virtual BindlessHandle GetOrCreateBindlessView(BufferBindingUsage usage, u32 stride, RHIDescriptorPool& descriptorPool) override;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;

    ID3D12Resource* GetRawBuffer()
    {
        return buffer.Get();
    }

private:
    virtual UniqueHandle<RHIBuffer> CreateStagingBuffer() override;

    ComPtr<DX12Device> device;

    ComPtr<ID3D12Resource> buffer;

    std::unordered_map<BufferViewCacheKey, BindlessHandle> viewCache;
};

} // namespace vex::dx12
