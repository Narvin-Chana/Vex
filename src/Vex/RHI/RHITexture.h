#pragma once

#include <Vex/EnumFlags.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>

namespace vex
{

// clang-format off

BEGIN_VEX_ENUM_FLAGS(RHITextureState, u8)
    Common = 0,
    RenderTarget = 1,
    UnorderedAccess = 2,
    DepthWrite = 4,
    DepthRead = 8,
    ShaderResource = 16,
    CopySource = 32,
    CopyDest = 64,
    Present = 128,
END_VEX_ENUM_FLAGS();

// clang-format on

class RHITexture
{
public:
    virtual ~RHITexture() = default;

    const TextureDescription& GetDescription() const
    {
        return description;
    }

    RHITextureState::Flags GetCurrentState() const
    {
        return currentState;
    }

    void SetCurrentState(RHITextureState::Flags newState)
    {
        currentState = newState;
    }

protected:
    TextureDescription description;
    RHITextureState::Flags currentState = RHITextureState::Common;
};

} // namespace vex