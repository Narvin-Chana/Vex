#include "DX12Texture.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex/Bindings.h>
#include <Vex/Logger.h>
#include <Vex/Platform/Windows/WString.h>
#include <Vex/RHI/RHIDescriptorPool.h>

#include <DX12/DX12Formats.h>
#include <DX12/HRChecker.h>

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
    // TODO: implement DSV logic
    VEX_NOT_YET_IMPLEMENTED();
    return {};
}

static D3D12_SHADER_RESOURCE_VIEW_DESC CreateShaderResourceViewDesc(DX12TextureView view)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC desc{
        .Format = view.format,
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

DX12Texture::DX12Texture(ComPtr<DX12Device>& device, const TextureDescription& desc)
    : srvUavHeap(device, MaxViewCountPerHeap)
    , rtvHeap(device, MaxViewCountPerHeap)
    , dsvHeap(device, MaxViewCountPerHeap)
{
    description = desc;

    CD3DX12_RESOURCE_DESC texDesc;
    switch (description.type)
    {
    case TextureType::TextureCube:
        VEX_ASSERT(description.depthOrArraySize == 6, "A texture cube must have a depthOrArraySize of 6.");
    case TextureType::Texture2D:
        texDesc = CD3DX12_RESOURCE_DESC::Tex2D(TextureFormatToDXGI(description.format),
                                               description.width,
                                               description.height,
                                               description.depthOrArraySize,
                                               description.mips);
        break;
    case TextureType::Texture3D:
        texDesc = CD3DX12_RESOURCE_DESC::Tex3D(TextureFormatToDXGI(description.format),
                                               description.width,
                                               description.height,
                                               description.depthOrArraySize,
                                               description.mips);
        break;
    }

    if (!(description.usage & ResourceUsage::Read))
    {
        texDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }
    if (description.usage & ResourceUsage::RenderTarget)
    {
        texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (description.usage & ResourceUsage::UnorderedAccess)
    {
        texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    if (description.usage & ResourceUsage::DepthStencil)
    {
        texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }

    static const D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_CLEAR_VALUE* clearValue = nullptr;
    if (description.clearValue.flags != TextureClear::None)
    {
        *clearValue = {};
        clearValue->Format = texDesc.Format;
        std::memcpy(clearValue->Color, description.clearValue.color, sizeof(clearValue->Color));
        clearValue->DepthStencil = { .Depth = description.clearValue.depth, .Stencil = description.clearValue.stencil };
    }

    // For SRGB handling in DX12, the texture should have a typeless format.
    // We then decide when creating the SRV/RTV if we want automatic SRGB conversions or not (via the SRV/RTV's format).
    if (FormatHasSRGBEquivalent(description.format))
    {
        texDesc.Format = GetTypelessFormatForSRGBCompatibleDX12Format(texDesc.Format);
    }

    chk << device->CreateCommittedResource(&heapProps,
                                           D3D12_HEAP_FLAG_NONE,
                                           &texDesc,
                                           D3D12_RESOURCE_STATE_COMMON,
                                           clearValue,
                                           IID_PPV_ARGS(&texture));

    texture->SetName(StringToWString(description.name).data());
}

DX12Texture::DX12Texture(ComPtr<DX12Device>& device, std::string name, ComPtr<ID3D12Resource> nativeTex)
    : texture(std::move(nativeTex))
    , rtvHeap(device, MaxViewCountPerHeap)
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
    description.usage = ResourceUsage::None;
    if (!(nativeDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
    {
        description.usage |= ResourceUsage::Read;
    }
    if (nativeDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {
        description.usage |= ResourceUsage::RenderTarget;
    }
    if (nativeDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    {
        description.usage |= ResourceUsage::UnorderedAccess;
    }
    if (nativeDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    {
        description.usage |= ResourceUsage::DepthStencil;
    }

    texture->SetName(StringToWString(description.name).data());
}

DX12Texture::~DX12Texture()
{
}

void DX12Texture::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    for (auto& [heapSlot, bindlessHandle] : cache | std::views::values)
    {
        if (bindlessHandle != GInvalidBindlessHandle)
        {
            reinterpret_cast<DX12DescriptorPool&>(descriptorPool).FreeStaticDescriptor(bindlessHandle);
        }
    }
    cache.clear();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12Texture::GetOrCreateRTVDSVView(ComPtr<DX12Device>& device, DX12TextureView view)
{
    using namespace Texture_Internal;

    bool isRTVView = (view.type == ResourceUsage::RenderTarget) && (description.usage & ResourceUsage::RenderTarget);
    bool isDSVView = (view.type == ResourceUsage::DepthStencil) && (description.usage & ResourceUsage::DepthStencil);
    VEX_ASSERT(isRTVView || isDSVView,
               "Texture view requested must be for an RTV or DSV AND the underlying texture must support this usage.");

    if (auto it = cache.find(view); it != cache.end())
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
        cache[view] = { .heapSlot = idx };
        auto desc = CreateRenderTargetViewDesc(view);
        auto rtvDescriptor = rtvHeap.GetCPUDescriptorHandle(idx);
        device->CreateRenderTargetView(texture.Get(), &desc, rtvDescriptor);
        return rtvDescriptor;
    }
    else // if (isDSVView)
    {
        auto idx = dsvHeapAllocator.Allocate();
        cache[view] = { .heapSlot = idx };
        auto desc = CreateDepthStencilViewDesc(view);
        auto dsvDescriptor = dsvHeap.GetCPUDescriptorHandle(idx);
        device->CreateDepthStencilView(texture.Get(), &desc, dsvDescriptor);

        return dsvDescriptor;
    }
}

BindlessHandle DX12Texture::GetOrCreateBindlessView(ComPtr<DX12Device>& device,
                                                    DX12TextureView view,
                                                    DX12DescriptorPool& descriptorPool)
{
    using namespace Texture_Internal;

    bool isSRVView = (view.type == ResourceUsage::Read) && (description.usage & ResourceUsage::Read);
    bool isUAVView =
        (view.type == ResourceUsage::UnorderedAccess) && (description.usage & ResourceUsage::UnorderedAccess);

    VEX_ASSERT(isSRVView || isUAVView,
               "Texture view requested must be of type SRV or UAV AND the underlying texture must support this usage.");

    if (auto it = cache.find(view); it != cache.end() && descriptorPool.IsValid(it->second.bindlessHandle))
    {
        return it->second.bindlessHandle;
    }

    if (isSRVView)
    {
        auto idx = srvUavHeapAllocator.Allocate();
        auto desc = CreateShaderResourceViewDesc(view);
        auto descriptor = srvUavHeap.GetCPUDescriptorHandle(idx);
        device->CreateShaderResourceView(texture.Get(), &desc, descriptor);
        BindlessHandle handle = descriptorPool.AllocateStaticDescriptor(*this);
        descriptorPool.CopyDescriptor(device, handle, descriptor);
        cache[view] = { .heapSlot = idx, .bindlessHandle = handle };

        return handle;
    }
    else // if (isUAVView)
    {
        auto idx = srvUavHeapAllocator.Allocate();
        auto desc = CreateUnorderedAccessViewDesc(view);
        auto descriptor = srvUavHeap.GetCPUDescriptorHandle(idx);
        device->CreateUnorderedAccessView(texture.Get(), nullptr, &desc, descriptor);
        BindlessHandle handle = descriptorPool.AllocateStaticDescriptor(*this);
        descriptorPool.CopyDescriptor(device, handle, descriptor);
        cache[view] = { .heapSlot = idx, .bindlessHandle = handle };

        return handle;
    }
}

DX12TextureView::DX12TextureView(const ResourceBinding& binding,
                                 const TextureDescription& description,
                                 ResourceUsage::Type usage)
    : type{ usage }
    , dimension{ TextureUtil::GetTextureViewType(binding) }
    , format{ TextureFormatToDXGI(TextureUtil::GetTextureFormat(binding)) }
    , mipBias{ binding.mipBias }
    , mipCount{ (binding.mipCount == 0) ? description.mips : binding.mipCount }
    , startSlice{ binding.startSlice }
    , sliceCount{ (binding.sliceCount == 0) ? description.depthOrArraySize : binding.sliceCount }
{
}

} // namespace vex::dx12