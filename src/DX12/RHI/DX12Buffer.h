#pragma once

#include <unordered_map>

#include <Vex/NonNullPtr.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>

#include <RHI/RHIAllocator.h>
#include <RHI/RHIBuffer.h>

#include <DX12/DX12DescriptorHeap.h>
#include <DX12/DX12Headers.h>

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

class DX12Buffer final : public RHIBufferBase
{
public:
    DX12Buffer(ComPtr<DX12Device>& device, RHIAllocator& allocator, const BufferDescription& desc);

    virtual std::span<u8> Map() override;
    virtual void Unmap() override;

    virtual BindlessHandle GetOrCreateBindlessView(BufferBindingUsage usage,
                                                   u32 stride,
                                                   RHIDescriptorPool& descriptorPool) override;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;
    virtual void FreeAllocation(RHIAllocator& allocator) override;

    void WriteShaderTable(std::span<void*> shaderIdentifiers,
                          u64 shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
                          u64 recordStride = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    ID3D12Resource* GetRawBuffer()
    {
        return buffer.Get();
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const
    {
        return buffer->GetGPUVirtualAddress();
    }

    u64 GetShaderTableStride() const
    {
        return shaderTableStride;
    }

private:
    virtual UniqueHandle<RHIBuffer> CreateStagingBuffer() override;

    ComPtr<DX12Device> device;
    NonNullPtr<RHIAllocator> allocator;

    ComPtr<ID3D12Resource> buffer;

    std::unordered_map<BufferViewCacheKey, BindlessHandle> viewCache;

    // Always should be defined.
    Allocation allocation;

    u64 shaderTableStride = 0;
};

} // namespace vex::dx12
