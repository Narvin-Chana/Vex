#pragma once

#include <Vex/Texture.h>

namespace vex
{

class RHITexture
{
public:
    virtual ~RHITexture() = default;

    const TextureDescription& GetDescription() const
    {
        return description;
    }

protected:
    TextureDescription description;
};

} // namespace vex