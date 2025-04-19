#pragma once

#include <Vex/Texture.h>

namespace vex
{

class RHITexture
{
public:
    virtual ~RHITexture() = default;

protected:
    TextureDescription description;
};

} // namespace vex