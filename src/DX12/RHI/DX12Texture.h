#pragma once

#include <unordered_map>

#include <Vex/Containers/FreeList.h>
#include <Vex/Resource.h>
#include <Vex/Texture.h>

#include <RHI/RHIFwd.h>
#include <RHI/RHITexture.h>

#include <DX12/DX12DescriptorHeap.h>
#include <DX12/DX12Headers.h>

namespace vex::dx12
{

// Uses the underlying resource's format if set to DXGI_FORMAT_UNKNOWN (and if the texture's format is not TYPELESS).
DXGI_FORMAT TextureViewToDXGIFormat(const TextureDesc& desc, const TextureViewDesc& view);

class DX12Texture final : public RHITextureBase
{
public:
    DX12Texture(ComPtr<DX12Device>& device, RHIAllocator& allocator, const TextureDesc& desc);
    // Takes ownership of the passed in texture.
    DX12Texture(ComPtr<DX12Device>& device, std::string name, ComPtr<ID3D12Resource> rawTex);

    virtual BindlessHandle GetOrCreateBindlessView(const TextureViewDesc& view,
                                                   RHIDescriptorPool& descriptorPool) override;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) override;
    virtual void FreeAllocation(RHIAllocator& allocator) override;

    ID3D12Resource* GetRawTexture() const
    {
        return texture.Get();
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetOrCreateRTVDSVView(const TextureViewDesc& view);

private:
    ComPtr<ID3D12Resource> texture;

    ComPtr<DX12Device> device;

    struct CacheEntry
    {
        u32 heapSlot = ~0U;
        BindlessHandle bindlessHandle = GInvalidBindlessHandle;
    };

    std::unordered_map<TextureViewDesc, CacheEntry> viewCache;

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
