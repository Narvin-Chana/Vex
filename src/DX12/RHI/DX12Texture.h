#pragma once

#include <unordered_map>

#include <Vex/Containers/FreeList.h>
#include <Vex/Hash.h>
#include <Vex/RHIFwd.h>
#include <Vex/Resource.h>

#include <RHI/RHITexture.h>

#include <DX12/DX12DescriptorHeap.h>
#include <DX12/DX12Headers.h>
#include <DX12/RHI/DX12DescriptorPool.h>

namespace vex
{
struct ResourceBinding;
}

namespace vex::dx12
{

struct DX12TextureView
{
    DX12TextureView(const ResourceBinding& binding, const TextureDescription& description, TextureUsage::Type usage);
    TextureUsage::Type usage;
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
    VEX_HASH_COMBINE(seed, obj.usage);
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

class DX12Texture final : public RHITextureInterface
{
public:
    DX12Texture(ComPtr<DX12Device>& device, const TextureDescription& description);
    // Takes ownership of the passed in texture.
    DX12Texture(ComPtr<DX12Device>& device, std::string name, ComPtr<ID3D12Resource> rawTex);
    ~DX12Texture();
    virtual BindlessHandle GetOrCreateBindlessView(const ResourceBinding& binding,
                                                   TextureUsage::Type usage,
                                                   RHIDescriptorPool& descriptorPool) override;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;
    virtual RHITextureState::Type GetClearTextureState() override;

    ID3D12Resource* GetRawTexture()
    {
        return texture.Get();
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetOrCreateRTVDSVView(DX12TextureView view);

    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

private:
    ComPtr<ID3D12Resource> texture;

    ComPtr<DX12Device> device;

    struct CacheEntry
    {
        u32 heapSlot = ~0U;
        BindlessHandle bindlessHandle = GInvalidBindlessHandle;
    };

    std::unordered_map<DX12TextureView, CacheEntry> viewCache;

    static constexpr u32 MaxViewCountPerHeap = 32;

    // CPU-only visible heaps are "free" to create.
    // Aka they are just CPU memory, requiring no GPU calls.
    DX12DescriptorHeap<HeapType::RTV> rtvHeap;
    DX12DescriptorHeap<HeapType::DSV> dsvHeap;

    FreeListAllocator rtvHeapAllocator{ MaxViewCountPerHeap };
    FreeListAllocator dsvHeapAllocator{ MaxViewCountPerHeap };
};

} // namespace vex::dx12
