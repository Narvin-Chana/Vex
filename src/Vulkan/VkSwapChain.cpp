#include "VkSwapChain.h"

namespace vex::vk
{

void VkSwapChain::Present()
{
}

void VkSwapChain::Resize(u32 width, u32 height)
{
}

void VkSwapChain::SetVSync(bool enableVSync)
{
}

bool VkSwapChain::NeedsFlushForVSyncToggle()
{
    return true;
}

RHITexture* VkSwapChain::GetBackBuffer(u8 backBufferIndex)
{
    return nullptr;
}

} // namespace vex::vk