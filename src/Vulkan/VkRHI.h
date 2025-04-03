#pragma once

#include <Vex/RHI.h>
#include <Vulkan/VkFeatureChecker.h>
#include <Vulkan/VkHeaders.h>

namespace vex
{
struct PlatformWindow;
}

namespace vex::vk
{

struct RHICreateInfo
{
    std::vector<const char*> additionalExtensions;
    std::vector<const char*> additionalLayers;
};

class VkRHI : public vex::RHI
{
    ::vk::UniqueInstance instance;
    ::vk::UniqueSurfaceKHR surface;

public:
    VkRHI(const RHICreateInfo& createInfo = {});
    virtual ~VkRHI() override;

    virtual void InitWindow(const PlatformWindow& window) override;
    virtual FeatureChecker& GetFeatureChecker() override;

private:
    VkFeatureChecker featureChecker;
};

} // namespace vex::vk