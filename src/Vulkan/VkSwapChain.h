#pragma once

#include <Vex/RHI/RHISwapChain.h>

namespace vex::vk
{

class VkSwapChain : public RHISwapChain
{
public:
    virtual void Present() override;
    virtual void Resize(u32 width, u32 height) override;

    virtual void SetVSync(bool enableVSync) override;
    virtual bool NeedsFlushForVSyncToggle() override;

    virtual RHITexture* GetBackBuffer(u8 backBufferIndex) override;
};

} // namespace vex::vk