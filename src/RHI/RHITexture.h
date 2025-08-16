#pragma once

#include <Vex/EnumFlags.h>
#include <Vex/RHIFwd.h>
#include <Vex/Resource.h>
#include <Vex/Texture.h>
#include <Vex/Types.h>

namespace vex
{

// clang-format off

BEGIN_VEX_ENUM_FLAGS(RHITextureState, u8)
    Common          = 0,
    RenderTarget    = 1 << 0,
    ShaderReadWrite = 1 << 1,
    DepthWrite      = 1 << 2,
    DepthRead       = 1 << 3,
    ShaderResource  = 1 << 4,
    CopySource      = 1 << 5,
    CopyDest        = 1 << 6,
    Present         = 1 << 7,
END_VEX_ENUM_FLAGS();

// clang-format on

class RHITextureBase
{
public:
    RHITextureBase() = default;
    RHITextureBase(const RHITextureBase&) = delete;
    RHITextureBase& operator=(const RHITextureBase&) = delete;
    RHITextureBase(RHITextureBase&&) = default;
    RHITextureBase& operator=(RHITextureBase&&) = default;
    ~RHITextureBase() = default;

    virtual BindlessHandle GetOrCreateBindlessView(const TextureBinding& binding,
                                                   TextureUsage::Type usage,
                                                   RHIDescriptorPool& descriptorPool) = 0;
    virtual void FreeBindlessHandles(RHIDescriptorPool& descriptorPool) = 0;
    virtual RHITextureState::Type GetClearTextureState() = 0;

    const TextureDescription& GetDescription() const
    {
        return description;
    }

    [[nodiscard]] RHITextureState::Flags GetCurrentState() const
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