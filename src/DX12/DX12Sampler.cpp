#include "DX12Sampler.h"

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

D3D12_SAMPLER_DESC GetDX12SamplerDescFromTextureSampler(const TextureSampler& sampler)
{
    return {
        .Filter = GetDX12FilterFromFilterMode(sampler.minFilter,
                                              sampler.magFilter,
                                              sampler.mipFilter,
                                              sampler.compareOp != CompareOp::Never),
        .AddressU = GetDX12TextureAddressModeFromAddressMode(sampler.addressU),
        .AddressV = GetDX12TextureAddressModeFromAddressMode(sampler.addressV),
        .AddressW = GetDX12TextureAddressModeFromAddressMode(sampler.addressW),
        .MipLODBias = sampler.mipLODBias,
        .MaxAnisotropy = sampler.maxAnisotropy,
        .ComparisonFunc = GraphicsPipeline::GetD3D12ComparisonFuncFromCompareOp(sampler.compareOp),
        // TODO: not implemented!
        // .BorderColor = GetDX12StaticBorderColorFromBorderColor(sampler.borderColor),
        .MinLOD = sampler.minLOD,
        .MaxLOD = sampler.maxLOD,
    };
}

} // namespace GraphicsPipeline

} // namespace vex::dx12