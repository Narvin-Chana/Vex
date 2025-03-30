#pragma once

#include <Vex/RHI.h>
#include <Vulkan/VkFeatureChecker.h>
#include <Vulkan/VkHeaders.h>

namespace vex
{
struct PlatformWindowHandle;
}

namespace vex::vk
{

class VkRHI : public vex::RHI
{
public:
    VkRHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation);
    virtual ~VkRHI() override;

    virtual std::vector<UniqueHandle<PhysicalDevice>> EnumeratePhysicalDevices() override;
    virtual void Init(const UniqueHandle<PhysicalDevice>& physicalDevice) override;

private:
    void InitWindow(const PlatformWindowHandle& windowHandle);

    ::vk::UniqueInstance instance;
    ::vk::UniqueSurfaceKHR surface;
    ::vk::UniqueDevice device;

    ::vk::Queue copyQueue;
    ::vk::Queue graphicsQueue;
    ::vk::Queue computeQueue;
};

} // namespace vex::vk