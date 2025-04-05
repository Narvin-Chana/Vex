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
    ::vk::UniqueInstance instance;
    ::vk::UniqueSurfaceKHR surface;

public:
    VkRHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation);
    virtual ~VkRHI() override;

    virtual FeatureChecker& GetFeatureChecker() override;

private:
    void InitWindow(const PlatformWindowHandle& windowHandle);

    VkFeatureChecker featureChecker;
};

} // namespace vex::vk