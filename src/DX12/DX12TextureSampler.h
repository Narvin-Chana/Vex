#pragma once

#include <span>
#include <vector>

#include <Vex/TextureSampler.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

namespace GraphicsPipeline
{

D3D12_TEXTURE_ADDRESS_MODE GetDX12TextureAddressModeFromAddressMode(AddressMode addressMode);
D3D12_STATIC_BORDER_COLOR GetDX12StaticBorderColorFromBorderColor(BorderColor borderColor);
D3D12_FILTER GetDX12FilterFromFilterMode(FilterMode minFilter,
                                         FilterMode magFilter,
                                         FilterMode mipFilter,
                                         bool useComparison = false);
std::vector<D3D12_STATIC_SAMPLER_DESC> GetDX12StaticSamplersFromTextureSamplers(std::span<TextureSampler> samplers);

} // namespace GraphicsPipeline

} // namespace vex::dx12