#include "DX12Texture.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex/Bindings.h>
#include <Vex/Logger.h>
#include <Vex/RHIImpl/RHIAllocator.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>

#include <RHI/RHIDescriptorPool.h>

#include <DX12/DX12Formats.h>
#include <DX12/HRChecker.h>

#define VEX_USE_CUSTOM_ALLOCATOR_TEXTURES 1

namespace vex::dx12
{

namespace Texture_Internal
{

static D3D12_RESOURCE_DIMENSION ConvertTypeToDX12ResourceDimension(TextureType type)
{
    switch (type)
    {
    case TextureType::Texture2D:
    case TextureType::TextureCube:
        return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    case TextureType::Texture3D:
        return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    default:
        VEX_LOG(Fatal, "Invalid texture type passed to D3D12_RESOURCE_DIMENSION.");
    }
    std::unreachable();
}

static D3D12_RENDER_TARGET_VIEW_DESC CreateRenderTargetViewDesc(DX12TextureView view)
{
    D3D12_RENDER_TARGET_VIEW_DESC desc{ .Format = view.format };

    switch (view.dimension)
    {
    case TextureViewType::Texture2D:
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Texture2D = {
            .MipSlice = view.mipBias,
            .PlaneSlice = view.startSlice,
        };
        break;
    case TextureViewType::Texture2DArray:
    case TextureViewType::TextureCube:
    case TextureViewType::TextureCubeArray:
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray = {
            .MipSlice = view.mipBias,
            .FirstArraySlice = view.startSlice,
            .ArraySize = view.sliceCount,
            .PlaneSlice = 0,
        };
        break;
    case TextureViewType::Texture3D:
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
        desc.Texture3D = {
            .MipSlice = view.mipBias,
            .FirstWSlice = view.startSlice,
            .WSize = view.sliceCount,
        };
        break;
    }

    return desc;
}

static D3D12_DEPTH_STENCIL_VIEW_DESC CreateDepthStencilViewDesc(DX12TextureView view)
{
    // TODO: could eventually investigate setting the DepthRead / StencilRead flags for further optimization.
    D3D12_DEPTH_STENCIL_VIEW_DESC desc{ .Format = view.format,
                                        .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                                        .Texture2D = { .MipSlice = view.mipBias } };
    return desc;
}

static D3D12_SHADER_RESOURCE_VIEW_DESC CreateShaderResourceViewDesc(DX12TextureView view)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC desc{
        .Format = GetDX12FormatForShaderResourceViewFormat(view.format),
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    };

    switch (view.dimension)
    {
    case TextureViewType::Texture2D:
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        // All mips
        desc.Texture2D = {
            .MostDetailedMip = view.mipBias,
            .MipLevels = view.mipCount,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0,
        };
        break;
    case TextureViewType::Texture2DArray:
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        // All mips and all slices
        desc.Texture2DArray = {
            .MostDetailedMip = view.mipBias,
            .MipLevels = view.mipCount,
            .FirstArraySlice = view.startSlice,
            .ArraySize = view.sliceCount,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0,
        };
        break;
    case TextureViewType::TextureCube:
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        desc.TextureCube = {
            .MostDetailedMip = view.mipBias,
            .MipLevels = view.mipCount,
            .ResourceMinLODClamp = 0,
        };
        break;
    case TextureViewType::TextureCubeArray:
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
        desc.TextureCubeArray = {
            .MostDetailedMip = view.mipBias,
            .MipLevels = view.mipCount,
            .First2DArrayFace = view.startSlice,
            .NumCubes = view.sliceCount,
            .ResourceMinLODClamp = 0,
        };
        break;
    case TextureViewType::Texture3D:
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        desc.Texture3D = {
            .MostDetailedMip = view.mipBias,
            .MipLevels = view.mipCount,
            .ResourceMinLODClamp = 0,
        };
        break;
    }

    return desc;
}

static D3D12_UNORDERED_ACCESS_VIEW_DESC CreateUnorderedAccessViewDesc(DX12TextureView view)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc{ .Format = view.format };

    switch (view.dimension)
    {
    case TextureViewType::Texture2D:
        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        // Write to first mip slice
        desc.Texture2D = {
            .MipSlice = view.mipBias,
            .PlaneSlice = 0,
        };
        break;
    case TextureViewType::Texture2DArray:
    case TextureViewType::TextureCube:
        // UAVs for TextureCube do not exist in D3D12, instead the user is expected to bind their texture cube as a
        // RWTexture2DArray.
        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray = {
            .MipSlice = view.mipBias,
            .FirstArraySlice = view.startSlice,
            .ArraySize = view.sliceCount,
            .PlaneSlice = 0,
        };
        break;
    case TextureViewType::TextureCubeArray:
        // For cube array, we can use texture2d array with 6x the array size
        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray = {
            .MipSlice = view.mipBias,
            // Each cube face has 6 faces
            .FirstArraySlice = view.startSlice * 6,
            .ArraySize = view.sliceCount * 6,
            .PlaneSlice = 0,
        };
        break;
    case TextureViewType::Texture3D:
        desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
        desc.Texture3D = {
            .MipSlice = view.mipBias,
            .FirstWSlice = view.startSlice,
            .WSize = view.sliceCount,
        };
        break;
    default:
        // Provide a safe fallback or throw a proper exception
        VEX_LOG(Fatal,
                "Unsupported texture dimension type for UAV creation: %d",
                magic_enum::enum_name(view.dimension));
        break;
    }

    return desc;
}

} // namespace Texture_Internal

DX12Texture::DX12Texture(ComPtr<DX12Device>& device, RHIAllocator& allocator, const TextureDescription& desc)
    : RHITextureBase(allocator)
    , texture(nullptr)
    , device(device)
{
    description = desc;

    CD3DX12_RESOURCE_DESC texDesc;
    switch (description.type)
    {
    case TextureType::TextureCube:
    case TextureType::Texture2D:
    {
        texDesc = CD3DX12_RESOURCE_DESC::Tex2D(TextureFormatToDXGI(description.format),
                                               description.width,
                                               description.height,
                                               description.GetArraySize(),
                                               description.mips);
        break;
    }
    case TextureType::Texture3D:
        texDesc = CD3DX12_RESOURCE_DESC::Tex3D(TextureFormatToDXGI(description.format),
                                               description.width,
                                               description.height,
                                               description.depthOrArraySize,
                                               description.mips);
        break;
    }

    if (description.usage & TextureUsage::RenderTarget)
    {
        texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        rtvHeap = DX12DescriptorHeap<DX12HeapType::RTV>(device, MaxViewCountPerRTVHeap, description.name);
        rtvHeapAllocator = FreeListAllocator(MaxViewCountPerRTVHeap);
    }
    if (description.usage & TextureUsage::ShaderReadWrite)
    {
        texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    if (description.usage & TextureUsage::DepthStencil)
    {
        texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        dsvHeap = DX12DescriptorHeap<DX12HeapType::DSV>(device, MaxViewCountPerDSVHeap, description.name);
        dsvHeapAllocator = FreeListAllocator(MaxViewCountPerDSVHeap);
    }

    static const D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    std::optional<D3D12_CLEAR_VALUE> clearValue;
    if (description.clearValue.flags != TextureClear::None)
    {
        clearValue = D3D12_CLEAR_VALUE();
        clearValue->Format = texDesc.Format;
        std::memcpy(clearValue->Color, description.clearValue.color.data(), sizeof(float) * 4);
        clearValue->DepthStencil = { .Depth = description.clearValue.depth, .Stencil = description.clearValue.stencil };
    }

    // In order to allow for a depth stencil texture to be read as an SRV, it must have the equivalent typeless format
    // (converted to the equivalent typed/D_ format for the actual view).
    if (description.usage & TextureUsage::DepthStencil && description.usage & TextureUsage::ShaderRead)
    {
        texDesc.Format = GetTypelessFormatForDepthStencilCompatibleDX12Format(texDesc.Format);
    }
    // For SRGB handling in DX12, the texture should have a typeless format.
    // We then decide when creating the SRV/RTV if we want automatic SRGB conversions or not (via the SRV/RTV's format).
    else if (FormatHasSRGBEquivalent(description.format))
    {
        texDesc.Format = GetTypelessFormatForSRGBCompatibleDX12Format(texDesc.Format);
    }

#if VEX_USE_CUSTOM_ALLOCATOR_TEXTURES
    allocation = allocator.AllocateResource(texture,
                                            texDesc,
                                            description.memoryLocality,
                                            D3D12_RESOURCE_STATE_COMMON,
                                            clearValue);
#else
    chk << device->CreateCommittedResource(&heapProps,
                                           D3D12_HEAP_FLAG_NONE,
                                           &texDesc,
                                           D3D12_RESOURCE_STATE_COMMON,
                                           clearValue.has_value() ? &clearValue.value() : nullptr,
                                           IID_PPV_ARGS(&texture));
#endif

#if !VEX_SHIPPING
    chk << texture->SetName(
        StringToWString(std::format("{}: {}", magic_enum::enum_name(description.type), description.name)).data());
#endif
}

DX12Texture::DX12Texture(ComPtr<DX12Device>& device, std::string name, ComPtr<ID3D12Resource> nativeTex)
    : texture(std::move(nativeTex))
    , device(device)
{
    VEX_ASSERT(texture, "The texture passed in should be defined!");
    description.name = std::move(name);

    D3D12_RESOURCE_DESC nativeDesc = texture->GetDesc();
    if (nativeDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    {
        // Array size of 6 and TEXTURE2D resource dimension means we suppose the texture is a cubemap.
        if (nativeDesc.DepthOrArraySize == 6)
        {
            description.type = TextureType::TextureCube;
        }
        else
        {
            description.type = TextureType::Texture2D;
        }
    }
    else if (nativeDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    {
        description.type = TextureType::Texture3D;
    }
    else
    {
        VEX_LOG(Fatal, "Vex DX12 RHI does not support 1D textures.");
        return;
    }
    description.width = static_cast<u32>(nativeDesc.Width);
    description.height = nativeDesc.Height;
    description.depthOrArraySize = static_cast<u32>(nativeDesc.DepthOrArraySize);
    description.mips = nativeDesc.MipLevels;
    description.format = DXGIToTextureFormat(nativeDesc.Format);
    description.usage = TextureUsage::None;
    if (!(nativeDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
    {
        description.usage |= TextureUsage::ShaderRead;
    }
    if (nativeDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {
        description.usage |= TextureUsage::RenderTarget;
        rtvHeap = DX12DescriptorHeap<DX12HeapType::RTV>(device, MaxViewCountPerRTVHeap, description.name);
        rtvHeapAllocator = FreeListAllocator(MaxViewCountPerRTVHeap);
    }
    if (nativeDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    {
        description.usage |= TextureUsage::ShaderReadWrite;
    }
    if (nativeDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    {
        description.usage |= TextureUsage::DepthStencil;
        dsvHeap = DX12DescriptorHeap<DX12HeapType::DSV>(device, MaxViewCountPerDSVHeap, description.name);
        dsvHeapAllocator = FreeListAllocator(MaxViewCountPerDSVHeap);
    }

#if !VEX_SHIPPING
    chk << texture->SetName(
        StringToWString(std::format("{}: {}", magic_enum::enum_name(description.type), description.name)).data());
#endif
}

BindlessHandle DX12Texture::GetOrCreateBindlessView(const TextureBinding& binding, RHIDescriptorPool& descriptorPool)
{
    DX12TextureView view{ binding };

    using namespace Texture_Internal;

    bool isSRVView = (view.usage == TextureUsage::ShaderRead) && (description.usage & TextureUsage::ShaderRead);
    bool isUAVView =
        (view.usage == TextureUsage::ShaderReadWrite) && (description.usage & TextureUsage::ShaderReadWrite);

    VEX_ASSERT(isSRVView || isUAVView,
               "Texture view requested must be of type SRV or UAV AND the underlying texture must support this usage.");

    // Check cache first
    if (auto it = viewCache.find(view); it != viewCache.end() && descriptorPool.IsValid(it->second.bindlessHandle))
    {
        return it->second.bindlessHandle;
    }

    BindlessHandle handle = descriptorPool.AllocateStaticDescriptor();

    if (isSRVView)
    {
        auto desc = CreateShaderResourceViewDesc(view);
        auto cpuDescriptorHandle = descriptorPool.GetCPUDescriptor(handle);
        device->CreateShaderResourceView(texture.Get(), &desc, cpuDescriptorHandle);
    }
    else // if (isUAVView)
    {
        auto desc = CreateUnorderedAccessViewDesc(view);
        auto cpuDescriptorHandle = descriptorPool.GetCPUDescriptor(handle);
        device->CreateUnorderedAccessView(texture.Get(), nullptr, &desc, cpuDescriptorHandle);
    }

    viewCache[view] = { .bindlessHandle = handle };
    return handle;
}

void DX12Texture::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    for (auto& [view, entry] : viewCache)
    {
        if (entry.bindlessHandle != GInvalidBindlessHandle)
        {
            descriptorPool.FreeStaticDescriptor(entry.bindlessHandle);
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

RHITextureBarrier DX12Texture::GetClearTextureBarrier()
{
    if (description.usage & TextureUsage::RenderTarget)
    {
        return RHITextureBarrier{ *this,
                                  RHIBarrierSync::RenderTarget,
                                  RHIBarrierAccess::RenderTarget,
                                  RHITextureLayout::RenderTarget };
    }

    if (description.usage & TextureUsage::DepthStencil)
    {
        return RHITextureBarrier{ *this,
                                  RHIBarrierSync::DepthStencil,
                                  RHIBarrierAccess::DepthStencilWrite,
                                  RHITextureLayout::DepthStencilWrite };
    }

    VEX_LOG(Fatal,
            "Invalid texture passed to ClearTexture, your texture must allow for either RenderTarget usage or "
            "DepthStencil usage.");
    std::unreachable();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12Texture::GetOrCreateRTVDSVView(DX12TextureView view)
{
    using namespace Texture_Internal;

    bool isRTVView = (view.usage == TextureUsage::RenderTarget) && (description.usage & TextureUsage::RenderTarget);
    bool isDSVView = (view.usage == TextureUsage::DepthStencil) && (description.usage & TextureUsage::DepthStencil);
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
        auto desc = CreateRenderTargetViewDesc(view);
        auto rtvDescriptor = rtvHeap.GetCPUDescriptorHandle(idx);
        device->CreateRenderTargetView(texture.Get(), &desc, rtvDescriptor);
        return rtvDescriptor;
    }
    else // if (isDSVView)
    {
        auto idx = dsvHeapAllocator.Allocate();
        viewCache[view] = { .heapSlot = idx };
        auto desc = CreateDepthStencilViewDesc(view);
        auto dsvDescriptor = dsvHeap.GetCPUDescriptorHandle(idx);
        device->CreateDepthStencilView(texture.Get(), &desc, dsvDescriptor);

        return dsvDescriptor;
    }
}

std::span<byte> DX12Texture::Map()
{
    VEX_NOT_YET_IMPLEMENTED();
    return {};
}

void DX12Texture::Unmap()
{
    VEX_NOT_YET_IMPLEMENTED();
}

DX12TextureView::DX12TextureView(const TextureBinding& binding)
    : usage{ binding.usage != TextureBindingUsage::None ? static_cast<TextureUsage::Type>(binding.usage)
                                                        : TextureUsage::None }
    , dimension{ TextureUtil::GetTextureViewType(binding) }
    , mipBias{ binding.mipBias }
    , mipCount{ (binding.mipCount == 0) ? binding.texture.description.mips : binding.mipCount }
    , startSlice{ binding.startSlice }
    , sliceCount{ (binding.sliceCount == 0) ? binding.texture.description.depthOrArraySize : binding.sliceCount }
{
    format = TextureFormatToDXGI(TextureUtil::GetTextureFormat(binding));
    if (binding.usage == TextureBindingUsage::ShaderRead)
    {
        if (binding.texture.description.usage & TextureUsage::DepthStencil &&
            binding.texture.description.usage & TextureUsage::ShaderRead)
        {
            format = GetTypelessFormatForDepthStencilCompatibleDX12Format(format);
        }
    }
}

} // namespace vex::dx12