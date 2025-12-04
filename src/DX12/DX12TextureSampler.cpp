#include "DX12TextureSampler.h"

#include <Vex/Logger.h>
#include <Vex/TextureSampler.h>

#include <DX12/DX12GraphicsPipeline.h>

namespace vex::dx12
{

namespace GraphicsPipeline
{

D3D12_TEXTURE_ADDRESS_MODE GetDX12TextureAddressModeFromAddressMode(AddressMode addressMode)
{
    return static_cast<D3D12_TEXTURE_ADDRESS_MODE>(std::to_underlying(addressMode) + 1);
}

D3D12_STATIC_BORDER_COLOR GetDX12StaticBorderColorFromBorderColor(BorderColor borderColor)
{
    switch (borderColor)
    {
    case BorderColor::TransparentBlackFloat:
    case BorderColor::TransparentBlackInt:
        return D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    case BorderColor::OpaqueBlackFloat:
        return D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    case BorderColor::OpaqueBlackInt:
        return D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK_UINT;
    case BorderColor::OpaqueWhiteFloat:
        return D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    case BorderColor::OpaqueWhiteInt:
        return D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE_UINT;
    default:
        VEX_LOG(Fatal, "Invalid border color passed to DX12 border color conversion function.");
    }
    std::unreachable();
}

D3D12_FILTER GetDX12FilterFromFilterMode(FilterMode minFilter,
                                         FilterMode magFilter,
                                         FilterMode mipFilter,
                                         bool useComparison)
{
    // D3D12 filter is a combination of min, mag, and mip filters
    if (minFilter == FilterMode::Anisotropic || magFilter == FilterMode::Anisotropic)
    {
        return useComparison ? D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC;
    }

    // Build the filter value based on min, mag, and mip filter combinations
    UINT filterValue = 0;

    // Min filter (bits 4-5)
    if (minFilter == FilterMode::Linear)
        filterValue |= 0x10;

    // Mag filter (bits 2-3)
    if (magFilter == FilterMode::Linear)
        filterValue |= 0x04;

    // Mip filter (bits 0-1)
    if (mipFilter == FilterMode::Linear)
        filterValue |= 0x01;

    // Add comparison bit if needed (bit 7)
    if (useComparison)
        filterValue |= 0x80;

    return static_cast<D3D12_FILTER>(filterValue);
}

std::vector<D3D12_STATIC_SAMPLER_DESC> GetDX12StaticSamplersFromTextureSamplers(Span<const TextureSampler> samplers)
{
    std::vector<D3D12_STATIC_SAMPLER_DESC> dxSamplers;
    dxSamplers.reserve(samplers.size());

    for (u32 i = 0; i < samplers.size(); ++i)
    {
        dxSamplers.push_back({
            .Filter = GetDX12FilterFromFilterMode(samplers[i].minFilter,
                                                  samplers[i].magFilter,
                                                  samplers[i].mipFilter,
                                                  samplers[i].compareOp != CompareOp::Never),
            .AddressU = GetDX12TextureAddressModeFromAddressMode(samplers[i].addressU),
            .AddressV = GetDX12TextureAddressModeFromAddressMode(samplers[i].addressV),
            .AddressW = GetDX12TextureAddressModeFromAddressMode(samplers[i].addressW),
            .MipLODBias = samplers[i].mipLODBias,
            .MaxAnisotropy = samplers[i].maxAnisotropy,
            .ComparisonFunc = GraphicsPipeline::GetD3D12ComparisonFuncFromCompareOp(samplers[i].compareOp),
            .BorderColor = GetDX12StaticBorderColorFromBorderColor(samplers[i].borderColor),
            .MinLOD = samplers[i].minLOD,
            .MaxLOD = samplers[i].maxLOD,
            .ShaderRegister = i,
            .RegisterSpace = 0,
            .ShaderVisibility = D3D12_SHADER_VISIBILITY::D3D12_SHADER_VISIBILITY_ALL,
        });
    }

    return dxSamplers;
}

} // namespace GraphicsPipeline

} // namespace vex::dx12