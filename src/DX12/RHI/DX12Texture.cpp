#include "DX12Texture.h"

#include <optional>
#include <ranges>

#include <Vex/Bindings.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHIImpl/RHIAllocator.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>
#include <Vex/RHIImpl/RHIPhysicalDevice.h>
#include <Vex/Utility/Formattable.h>
#include <Vex/Utility/WString.h>

#include <RHI/RHIDescriptorPool.h>

#include <DX12/DX12Formats.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

namespace Texture_Internal
{

static D3D12_RENDER_TARGET_VIEW_DESC CreateRenderTargetViewDesc(const TextureViewDesc& view, DXGI_FORMAT format)
{
    D3D12_RENDER_TARGET_VIEW_DESC desc{ .Format = format };

    switch (view.viewType)
    {
    case TextureViewType::Texture2D:
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Texture2D = {
            .MipSlice = view.subresource.startMip,
            .PlaneSlice = view.subresource.startSlice,
        };
        break;
    case TextureViewType::Texture2DArray:
    case TextureViewType::TextureCube:
    case TextureViewType::TextureCubeArray:
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray = {
            .MipSlice = view.subresource.startMip,
            .FirstArraySlice = view.subresource.startSlice,
            .ArraySize = view.subresource.sliceCount,
            .PlaneSlice = 0,
        };
        break;
    case TextureViewType::Texture3D:
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
        desc.Texture3D = {
            .MipSlice = view.subresource.startMip,
            .FirstWSlice = view.subresource.startSlice,
            .WSize = view.subresource.sliceCount,
        };
        break;
    default:
        VEX_LOG(Fatal, "Unsupported texture dimension type for RTV creation: %d", view.viewType);
        std::unreachable();
    }

    return desc;
}

static D3D12_DEPTH_STENCIL_VIEW_DESC CreateDepthStencilViewDesc(const TextureViewDesc& view, DXGI_FORMAT format)
{
    // TODO: could eventually investigate setting the DepthRead / StencilRead flags for further optimization.
    D3D12_DEPTH_STENCIL_VIEW_DESC desc{ .Format = format,
                                        .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                                        .Texture2D = { .MipSlice = view.subresource.startMip } };
    return desc;
}

static D3D12_SHADER_RESOURCE_VIEW_DESC CreateShaderResourceViewDesc(const TextureDesc& textureDesc,
                                                                    const TextureViewDesc& view,
                                                                    DXGI_FORMAT format)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC desc{
        .Format = GetDX12FormatForShaderResourceViewFormat(format, view.subresource.GetSingleAspect(textureDesc)),
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    };

    switch (view.viewType)
    {
    case TextureViewType::Texture2D:
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D = {
            .MostDetailedMip = view.subresource.startMip,
            .MipLevels = view.subresource.GetMipCount(textureDesc),
            .PlaneSlice = view.subresource.GetSingleAspect(textureDesc) == TextureAspect::Stencil ? 1u : 0u,
            .ResourceMinLODClamp = 0,
        };
        break;
    case TextureViewType::Texture2DArray:
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray = {
            .MostDetailedMip = view.subresource.startMip,
            .MipLevels = view.subresource.GetMipCount(textureDesc),
            .FirstArraySlice = view.subresource.startSlice,
            .ArraySize = view.subresource.GetSliceCount(textureDesc),
            .PlaneSlice = view.subresource.GetSingleAspect(textureDesc) == TextureAspect::Stencil ? 1u : 0u,
            .ResourceMinLODClamp = 0,
        };
        break;
    case TextureViewType::TextureCube:
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        desc.TextureCube = {
            .MostDetailedMip = view.subresource.startMip,
            .MipLevels = view.subresource.GetMipCount(textureDesc),
            .ResourceMinLODClamp = 0,
        };
        break;
    case TextureViewType::TextureCubeArray:
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
        desc.TextureCubeArray = {
            .MostDetailedMip = view.subresource.startMip,
            .MipLevels = view.subresource.GetMipCount(textureDesc),
            .First2DArrayFace = view.subresource.startSlice,
            .NumCubes = view.subresource.GetSliceCount(textureDesc) / GTextureCubeFaceCount,
            .ResourceMinLODClamp = 0,
        };
        break;
    case TextureViewType::Texture3D:
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        desc.Texture3D = {
            .MostDetailedMip = view.subresource.startMip,
            .MipLevels = view.subresource.GetMipCount(textureDesc),
            .ResourceMinLODClamp = 0,
        };
        break;
    default:
        VEX_LOG(Fatal, "Unsupported texture dimension type for SRV creation: %d", view.viewType);
        std::unreachable();
    }

    return desc;
}

static D3D12_UNORDERED_ACCESS_VIEW_DESC CreateUnorderedAccessViewDesc(const TextureDesc& textureDesc,
                                                                      const TextureViewDesc& view,
                                                                      DXGI_FORMAT format)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc{ .Format = GetNonSRGBEquivalentForSRGBCompatibleDX12Format(format) };

    switch (view.viewType)
    {
    case TextureViewType::Texture2D:
        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        // Write to first mip slice
        desc.Texture2D = {
            .MipSlice = view.subresource.startMip,
            .PlaneSlice = 0,
        };
        break;
    case TextureViewType::Texture2DArray:
    case TextureViewType::TextureCube:
    case TextureViewType::TextureCubeArray:
        // UAVs for TextureCube and TextureCubeArray do not exist in D3D12, instead the user is expected to bind their
        // texture cube as a RWTexture2DArray.
        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray = {
            .MipSlice = view.subresource.startMip,
            .FirstArraySlice = view.subresource.startSlice,
            .ArraySize = view.subresource.GetSliceCount(textureDesc),
            .PlaneSlice = 0,
        };
        break;
    case TextureViewType::Texture3D:
        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
        desc.Texture3D = {
            .MipSlice = view.subresource.startMip,
            .FirstWSlice = view.subresource.startSlice,
            .WSize = view.subresource.GetSliceCount(textureDesc),
        };
        break;
    default:
        VEX_LOG(Fatal, "Unsupported texture dimension type for UAV creation: %d", view.viewType);
        std::unreachable();
    }

    return desc;
}

} // namespace Texture_Internal

DXGI_FORMAT TextureViewToDXGIFormat(const TextureDesc& desc, const TextureViewDesc& view)
{
    DXGI_FORMAT dxgiFormat = TextureFormatToDXGI(desc.format, view.isSRGB);
    return view.usage == TextureUsage::ShaderRead &&
                   (desc.usage & TextureUsage::DepthStencil && desc.usage & TextureUsage::ShaderRead)
               ? GetTypelessFormatForDepthStencilCompatibleDX12Format(dxgiFormat)
               : dxgiFormat;
}

DX12Texture::DX12Texture(ComPtr<DX12Device>& device, RHIAllocator& allocator, const TextureDesc& desc)
    : RHITextureBase(allocator)
    , texture(nullptr)
    , device(device)
{
    this->desc = desc;

    CD3DX12_RESOURCE_DESC1 texDesc;
    switch (desc.type)
    {
    case TextureType::TextureCube:
    case TextureType::Texture2D:
    {
        texDesc = CD3DX12_RESOURCE_DESC1::Tex2D(TextureFormatToDXGI(desc.format, false),
                                                desc.width,
                                                desc.height,
                                                desc.GetSliceCount(),
                                                desc.mips);
        break;
    }
    case TextureType::Texture3D:
        texDesc = CD3DX12_RESOURCE_DESC1::Tex3D(TextureFormatToDXGI(desc.format, false),
                                                desc.width,
                                                desc.height,
                                                desc.depthOrSliceCount,
                                                desc.mips);
        break;
    default:
        VEX_LOG(Fatal, "Unsupported texture dimension type for texture creation: %d", desc.type);
        std::unreachable();
    }

    bool useFastTextureClear = false;

    if (desc.usage & TextureUsage::RenderTarget)
    {
        texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        rtvHeap = DX12DescriptorHeap<DX12HeapType::RTV>(device, InitialViewCountPerRTVHeap, desc.name);
        rtvHeapAllocator = FreeListAllocator(InitialViewCountPerRTVHeap);
        useFastTextureClear = true;
    }
    if (desc.usage & TextureUsage::ShaderReadWrite)
    {
        texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    if (desc.usage & TextureUsage::DepthStencil)
    {
        if (!(desc.usage & TextureUsage::ShaderRead))
        {
            texDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        }

        texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        dsvHeap = DX12DescriptorHeap<DX12HeapType::DSV>(device, InitialViewCountPerDSVHeap, desc.name);
        dsvHeapAllocator = FreeListAllocator(InitialViewCountPerDSVHeap);
        useFastTextureClear = true;
    }

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = texDesc.Format;
    std::memcpy(clearValue.Color, desc.clearValue.color.data(), sizeof(float) * 4);
    clearValue.DepthStencil = { .Depth = desc.clearValue.depth, .Stencil = desc.clearValue.stencil };

    static const D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    // In order to allow for a depth stencil texture to be read as an SRV, it must have the equivalent typeless format
    // (converted to the equivalent typed/D_ format for the actual view).
    if (desc.usage & TextureUsage::DepthStencil && desc.usage & TextureUsage::ShaderRead)
    {
        texDesc.Format = GetTypelessFormatForDepthStencilCompatibleDX12Format(texDesc.Format);
    }
    // For SRGB handling in DX12, the texture should have a typeless format.
    // We then decide when creating the SRV/RTV if we want automatic SRGB conversions or not (via the SRV/RTV's format).
    else if (FormatUtil::HasSRGBEquivalent(desc.format))
    {
        texDesc.Format = GetTypelessFormatForSRGBCompatibleDX12Format(texDesc.Format);
    }

    if (VEX_USE_CUSTOM_RESOURCE_ALLOCATOR && GPhysicalDevice->SupportsTightAlignment())
    {
        texDesc.Flags |= D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT;
    }

#if VEX_USE_CUSTOM_RESOURCE_ALLOCATOR
    allocation =
        allocator.AllocateResource(texture,
                                   texDesc,
                                   desc.memoryLocality,
                                   0,
                                   D3D12_BARRIER_LAYOUT_UNDEFINED,
                                   useFastTextureClear ? std::optional<D3D12_CLEAR_VALUE>(clearValue) : std::nullopt);
#else
    chk << device->CreateCommittedResource3(&heapProps,
                                            D3D12_HEAP_FLAG_NONE,
                                            &texDesc,
                                            D3D12_BARRIER_LAYOUT_UNDEFINED,
                                            useFastTextureClear ? &clearValue : nullptr,
                                            nullptr,
                                            0,
                                            nullptr,
                                            IID_PPV_ARGS(&texture));
#endif

#if !VEX_SHIPPING
    chk << texture->SetName(
        PlatformUtil::StringToWString(std::format("{}: {}", magic_enum::enum_name(desc.type), desc.name)).data());
#endif
}

DX12Texture::DX12Texture(ComPtr<DX12Device>& device, std::string name, ComPtr<ID3D12Resource> nativeTex)
    : texture(std::move(nativeTex))
    , device(device)
{
    VEX_ASSERT(texture, "The texture passed in should be defined!");
    desc.name = std::move(name);

    D3D12_RESOURCE_DESC nativeDesc = texture->GetDesc();
    if (nativeDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    {
        // Array size of 6 and TEXTURE2D resource dimension means we suppose the texture is a cubemap.
        if (nativeDesc.DepthOrArraySize == GTextureCubeFaceCount)
        {
            desc.type = TextureType::TextureCube;
        }
        else
        {
            desc.type = TextureType::Texture2D;
        }
    }
    else if (nativeDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    {
        desc.type = TextureType::Texture3D;
    }
    else
    {
        VEX_LOG(Fatal, "Vex DX12 RHI does not support 1D textures.");
        return;
    }
    desc.width = static_cast<u32>(nativeDesc.Width);
    desc.height = nativeDesc.Height;
    desc.depthOrSliceCount = static_cast<u32>(nativeDesc.DepthOrArraySize);
    desc.mips = nativeDesc.MipLevels;
    desc.format = DXGIToTextureFormat(nativeDesc.Format);
    desc.usage = TextureUsage::None;
    if (!(nativeDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
    {
        desc.usage |= TextureUsage::ShaderRead;
    }
    if (nativeDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {
        desc.usage |= TextureUsage::RenderTarget;
        rtvHeap = DX12DescriptorHeap<DX12HeapType::RTV>(device, InitialViewCountPerRTVHeap, desc.name);
        rtvHeapAllocator = FreeListAllocator(InitialViewCountPerRTVHeap);
    }
    if (nativeDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    {
        desc.usage |= TextureUsage::ShaderReadWrite;
    }
    if (nativeDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    {
        desc.usage |= TextureUsage::DepthStencil;
        dsvHeap = DX12DescriptorHeap<DX12HeapType::DSV>(device, InitialViewCountPerDSVHeap, desc.name);
        dsvHeapAllocator = FreeListAllocator(InitialViewCountPerDSVHeap);
    }

#if !VEX_SHIPPING
    chk << texture->SetName(
        PlatformUtil::StringToWString(std::format("{}: {}", magic_enum::enum_name(desc.type), desc.name)).data());
#endif
}

BindlessHandle DX12Texture::GetOrCreateBindlessView(const TextureViewDesc& view, RHIDescriptorPool& descriptorPool)
{
    using namespace Texture_Internal;

    bool isSRVView = (view.usage == TextureUsage::ShaderRead) && (desc.usage & TextureUsage::ShaderRead);
    bool isUAVView = (view.usage == TextureUsage::ShaderReadWrite) && (desc.usage & TextureUsage::ShaderReadWrite);

    VEX_ASSERT(isSRVView || isUAVView,
               "Texture view requested must be of type SRV or UAV AND the underlying texture must support this usage.");

    // Check cache first
    if (auto it = viewCache.find(view);
        it != viewCache.end() && descriptorPool.IsValid(DescriptorType::Resource, it->second.bindlessHandle))
    {
        return it->second.bindlessHandle;
    }

    BindlessHandle handle = descriptorPool.AllocateStaticDescriptor(DescriptorType::Resource);

    if (isSRVView)
    {
        auto srvDesc = CreateShaderResourceViewDesc(desc, view, TextureViewToDXGIFormat(desc, view));
        auto cpuDescriptorHandle = descriptorPool.GetCPUDescriptor(handle);
        device->CreateShaderResourceView(texture.Get(), &srvDesc, cpuDescriptorHandle);
    }
    else // if (isUAVView)
    {
        auto uavDesc = CreateUnorderedAccessViewDesc(desc, view, TextureViewToDXGIFormat(desc, view));
        auto cpuDescriptorHandle = descriptorPool.GetCPUDescriptor(handle);
        device->CreateUnorderedAccessView(texture.Get(), nullptr, &uavDesc, cpuDescriptorHandle);
    }

    viewCache[view] = { .bindlessHandle = handle };
    return handle;
}

void DX12Texture::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    for (auto& [_, bindlessHandle] : viewCache | std::views::values)
    {
        if (bindlessHandle != GInvalidBindlessHandle)
        {
            descriptorPool.FreeStaticDescriptor(DescriptorType::Resource, bindlessHandle);
        }
    }
    viewCache.clear();
}

void DX12Texture::FreeAllocation(RHIAllocator& allocator)
{
    if (allocation.has_value())
    {
        allocator.FreeResource(*allocation);
    }
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12Texture::GetOrCreateRTVDSVView(const TextureViewDesc& view)
{
    using namespace Texture_Internal;

    const bool isRTVView = (view.usage == TextureUsage::RenderTarget) && (desc.usage & TextureUsage::RenderTarget);
    const bool isDSVView = (view.usage == TextureUsage::DepthStencil) && (desc.usage & TextureUsage::DepthStencil);
    VEX_ASSERT(isRTVView || isDSVView,
               "Texture view requested must be for an RTV or DSV AND the underlying texture must support this usage.");

    if (auto it = viewCache.find(view); it != viewCache.end())
    {
        if (isRTVView)
        {
            return rtvHeap.GetCPUDescriptorHandle(it->second.heapSlot);
        }
        else // if (isDSVView)
        {
            return dsvHeap.GetCPUDescriptorHandle(it->second.heapSlot);
        }
    }

    // Generate and add to cache
    if (isRTVView)
    {
        auto idx = rtvHeapAllocator.Allocate();
        viewCache[view] = { .heapSlot = idx };
        auto rtvDesc = CreateRenderTargetViewDesc(view, TextureViewToDXGIFormat(desc, view));
        auto rtvDescriptor = rtvHeap.GetCPUDescriptorHandle(idx);
        device->CreateRenderTargetView(texture.Get(), &rtvDesc, rtvDescriptor);
        return rtvDescriptor;
    }
    else // if (isDSVView)
    {
        auto idx = dsvHeapAllocator.Allocate();
        viewCache[view] = { .heapSlot = idx };
        auto dsvDesc = CreateDepthStencilViewDesc(view, TextureViewToDXGIFormat(desc, view));
        auto dsvDescriptor = dsvHeap.GetCPUDescriptorHandle(idx);
        device->CreateDepthStencilView(texture.Get(), &dsvDesc, dsvDescriptor);

        return dsvDescriptor;
    }
}

} // namespace vex::dx12