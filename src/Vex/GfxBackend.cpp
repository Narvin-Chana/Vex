#include "GfxBackend.h"

#include <Vex/FeatureChecker.h>
#include <Vex/Logger.h>
#include <Vex/RHI.h>

namespace vex
{

GfxBackend::GfxBackend(UniqueHandle<RenderHardwareInterface>&& newRHI, const BackendDescription& description)
    : rhi(std::move(newRHI))
    , description(description)
{
    VEX_LOG(Info, "Graphics API Support:\n\tDX12: {}\n\tVulkan: {}", VEX_DX12, VEX_VULKAN);
    std::string vexTargetName;
    if (VEX_DEBUG)
    {
        vexTargetName = "Debug (no optimizations with debug symbols)";
    }
    else if (VEX_DEVELOPMENT)
    {
        vexTargetName = "Development (full optimizations with debug symbols)";
    }
    else if (VEX_SHIPPING)
    {
        vexTargetName = "Shipping (full optimizations with no debug symbols)";
    }
    VEX_LOG(Info, "Running Vex in {}", vexTargetName);

    auto physicalDevices = rhi->EnumeratePhysicalDevices();
    if (physicalDevices.empty())
    {
        VEX_LOG(Fatal, "The underlying graphics API was unable to find atleast one physical device.");
    }

    // Obtain the best physical device.
    std::sort(physicalDevices.begin(), physicalDevices.end(), [](const auto& l, const auto& r) { return *l > *r; });
#if !VEX_SHIPPING
    physicalDevices[0]->DumpPhysicalDeviceInfo();
#endif

    // Initializes RHI which includes creating logicial device and swapchain
    rhi->Init(physicalDevices[0]);

    VEX_LOG(Info,
            "Created graphics backend with width {} and height {}.",
            description.platformWindow.width,
            description.platformWindow.height);
}

GfxBackend::~GfxBackend() = default;

} // namespace vex
