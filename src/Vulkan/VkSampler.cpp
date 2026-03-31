#include "VkSampler.h"

namespace vex::vk
{

::vk::SamplerCreateInfo GraphicsPipeline::GetVkSamplerCreateInfoFromTextureSampler(const TextureSampler& sampler)
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
                                    .borderColor = BorderColorToVkBorderColor(sampler.borderColor),
                                    .unnormalizedCoordinates = false };
}

} // namespace vex::vk