#pragma once

#include <unordered_map>

#include <Vex/Containers/FreeList.h>
#include <Vex/Resource.h>
#include <Vex/Utility/Hash.h>

#include <RHI/RHIAllocator.h>
#include <RHI/RHIFwd.h>
#include <RHI/RHITexture.h>

#include <DX12/DX12DescriptorHeap.h>
#include <DX12/DX12Headers.h>

namespace vex
{
struct TextureBinding;
struct ResourceBinding;
} // namespace vex

namespace vex::dx12
{

struct DX12TextureView
{
    DX12TextureView(const TextureBinding& binding);
    NonNullPtr<TextureDesc> desc;
    TextureUsage::Type usage;
    TextureViewType dimension;
    // Uses the underlying resource's format if set to DXGI_FORMAT_UNKNOWN (and if the texture's format is not
    // TYPELESS!).
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    TextureSubresource subresource;

    constexpr bool operator==(const DX12TextureView&) const = default;
};

} // namespace vex::dx12

// clang-format off
VEX_MAKE_HASHABLE(vex::dx12::DX12TextureView,
    VEX_HASH_COMBINE(seed, obj.usage);
    VEX_HASH_COMBINE(seed, obj.dimension);
    VEX_HASH_COMBINE(seed, obj.format);
    VEX_HASH_COMBINE(seed, obj.subresource);
)
// clang-format on

namespace vex::dx12
{

class DX12Texture final : public RHITextureBase
{
public:
    DX12Texture(ComPtr<DX12Device>& device, RHIAllocator& allocator, const TextureDesc& desc);
    // Takes ownership of the passed in texture.
    DX12Texture(ComPtr<DX12Device>& device, std::string name, ComPtr<ID3D12Resource> rawTex);

    virtual BindlessHandle GetOrCreateBindlessView(const TextureBinding& binding,
                                                   RHIDescriptorPool& descriptorPool) override;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;
    virtual void FreeAllocation(RHIAllocator& allocator) override;

    ID3D12Resource* GetRawTexture()
    {
        return texture.Get();
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetOrCreateRTVDSVView(const DX12TextureView& view);

private:
    ComPtr<ID3D12Resource> texture;

    ComPtr<DX12Device> device;

    struct CacheEntry
    {
        u32 heapSlot = ~0U;
        BindlessHandle bindlessHandle = GInvalidBindlessHandle;
    };

    std::unordered_map<DX12TextureView, CacheEntry> viewCache;

    static constexpr u32 InitialViewCountPerRTVHeap = 2;
    static constexpr u32 InitialViewCountPerDSVHeap = 1;

    // CPU-only visible heaps are "free" to create.
    // Aka they are just CPU memory, requiring no GPU calls.
    DX12DescriptorHeap<DX12HeapType::RTV> rtvHeap;
    DX12DescriptorHeap<DX12HeapType::DSV> dsvHeap;

    FreeListAllocator32 rtvHeapAllocator;
    FreeListAllocator32 dsvHeapAllocator;

    // Can be nullopt in the case of swapchain backbuffers.
    std::optional<Allocation> allocation;
};

} // namespace vex::dx12
