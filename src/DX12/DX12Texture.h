#pragma once

#include <unordered_map>

#include <Vex/Containers/FreeList.h>
#include <Vex/Hash.h>
#include <Vex/RHI/RHITexture.h>

#include <DX12/DX12DescriptorHeap.h>
#include <DX12/DX12DescriptorPool.h>
#include <DX12/DX12Headers.h>

namespace vex
{
struct ResourceBinding;
}

namespace vex::dx12
{

struct DX12TextureView
{
    DX12TextureView(const ResourceBinding& binding, const TextureDescription& description, ResourceUsage::Type usage);
    ResourceUsage::Type type;
    TextureViewType dimension;
    // Uses the underlying resource's format if set to DXGI_FORMAT_UNKNOWN (and if the texture's format is not
    // TYPELESS!).
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    u32 mipBias;
    u32 mipCount;
    u32 startSlice;
    u32 sliceCount;

    bool operator==(const DX12TextureView&) const = default;
};

} // namespace vex::dx12

// clang-format off
VEX_MAKE_HASHABLE(vex::dx12::DX12TextureView,
    VEX_HASH_COMBINE(seed, obj.type);
    VEX_HASH_COMBINE(seed, obj.dimension);
    VEX_HASH_COMBINE(seed, obj.format);
    VEX_HASH_COMBINE(seed, obj.mipBias);
    VEX_HASH_COMBINE(seed, obj.mipCount);
    VEX_HASH_COMBINE(seed, obj.startSlice);
    VEX_HASH_COMBINE(seed, obj.sliceCount);
)
// clang-format on

namespace vex::dx12
{

class DX12Texture : public RHITexture
{
public:
    DX12Texture(ComPtr<DX12Device>& device, const TextureDescription& description);
    // Takes ownership of the passed in texture.
    DX12Texture(ComPtr<DX12Device>& device, std::string name, ComPtr<ID3D12Resource> rawTex);
    virtual ~DX12Texture() override;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;

    ID3D12Resource* GetRawTexture()
    {
        return texture.Get();
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetOrCreateRTVDSVView(ComPtr<DX12Device>& device, DX12TextureView view);
    BindlessHandle GetOrCreateBindlessView(ComPtr<DX12Device>& device,
                                           DX12TextureView view,
                                           DX12DescriptorPool& descriptorPool);

    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

private:
    ComPtr<ID3D12Resource> texture;

    struct CacheEntry
    {
        u32 heapSlot;
        BindlessHandle bindlessHandle = GInvalidBindlessHandle;
    };

    std::unordered_map<DX12TextureView, CacheEntry> cache;

    static constexpr u32 MaxViewCountPerHeap = 32;

    // CPU-only visible heaps are "free" to create.
    // Aka they are just CPU memory, requiring no GPU calls.
    DX12DescriptorHeap<HeapType::CBV_SRV_UAV> srvUavHeap;
    DX12DescriptorHeap<HeapType::RTV> rtvHeap;
    DX12DescriptorHeap<HeapType::DSV> dsvHeap;

    FreeListAllocator srvUavHeapAllocator{ MaxViewCountPerHeap };
    FreeListAllocator rtvHeapAllocator{ MaxViewCountPerHeap };
    FreeListAllocator dsvHeapAllocator{ MaxViewCountPerHeap };
};

} // namespace vex::dx12
