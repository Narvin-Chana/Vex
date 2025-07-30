#include "RHIResourceLayout.h"

#include <Vex/Logger.h>

namespace vex
{

void RHIResourceLayoutInterface::SetSamplers(std::span<TextureSampler> newSamplers)
{
    // Validation of the sampler name.
    for (auto& s : newSamplers)
    {
        if (s.name.empty())
        {
            VEX_LOG(Fatal,
                    "Your sampler state must have a valid name, otherwise you cannot use the sampler in shaders!");
        }
    }

    samplers = { newSamplers.begin(), newSamplers.end() };
    isDirty = true;
}

std::span<const TextureSampler> RHIResourceLayoutInterface::GetStaticSamplers() const
{
    return { samplers.begin(), samplers.end() };
}

} // namespace vex