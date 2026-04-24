#include "VkSampler.h"

namespace vex::vk
{

::vk::SamplerCreateInfo FillInVkSamplerCreateInfoWithTextureSamplerBase(const TextureSamplerBase& sampler)
{
    const bool useAnisotropy = sampler.minFilter == FilterMode::Anisotropic ||
                               sampler.magFilter == FilterMode::Anisotropic ||
                               sampler.mipFilter == FilterMode::Anisotropic;

    return ::vk::SamplerCreateInfo{ .magFilter = FilterModeToVkFilter(sampler.magFilter),
                                    .minFilter = FilterModeToVkFilter(sampler.minFilter),
                                    .mipmapMode = FilterModeToVkMipMapMode(sampler.mipFilter),
                                    .addressModeU = AddressModeToVkSamplerAddressMode(sampler.addressU),
                                    .addressModeV = AddressModeToVkSamplerAddressMode(sampler.addressV),
                                    .addressModeW = AddressModeToVkSamplerAddressMode(sampler.addressW),
                                    .mipLodBias = sampler.mipLODBias,
                                    .anisotropyEnable = useAnisotropy,
                                    .maxAnisotropy = static_cast<float>(sampler.maxAnisotropy),
                                    .compareEnable = sampler.compareOp != CompareOp::Never,
                                    .compareOp = static_cast<::vk::CompareOp>(sampler.compareOp),
                                    .minLod = sampler.minLOD,
                                    .maxLod = sampler.maxLOD,
                                    .unnormalizedCoordinates = false };
}

::vk::SamplerCreateInfo GraphicsPipeline::GetVkSamplerCreateInfoFromStaticTextureSampler(
    const StaticTextureSampler& sampler)
{
    ::vk::SamplerCreateInfo vkSamplerCreateInfo = FillInVkSamplerCreateInfoWithTextureSamplerBase(sampler);
    vkSamplerCreateInfo.borderColor = BorderColorToVkBorderColor(sampler.borderColor);
    return vkSamplerCreateInfo;
}
::vk::SamplerCreateInfo GraphicsPipeline::GetVkSamplerCreateInfoFromBindlessTextureSampler(
    const BindlessTextureSampler& sampler, ::vk::SamplerCustomBorderColorCreateInfoEXT& customBorder)
{
    ::vk::SamplerCreateInfo vkSamplerCreateInfo = FillInVkSamplerCreateInfoWithTextureSamplerBase(sampler);
    vkSamplerCreateInfo.borderColor = ::vk::BorderColor::eFloatCustomEXT;
    vkSamplerCreateInfo.pNext = &customBorder;
    customBorder.customBorderColor.setFloat32(sampler.borderColor);
    return vkSamplerCreateInfo;
}

} // namespace vex::vk