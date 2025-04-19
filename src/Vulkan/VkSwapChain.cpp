#include "VkSwapChain.h"

#include "VkTexture.h"

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

UniqueHandle<RHITexture> VkSwapChain::CreateBackBuffer(u8 backBufferIndex)
{
    return MakeUnique<VkTexture>();
}

} // namespace vex::vk