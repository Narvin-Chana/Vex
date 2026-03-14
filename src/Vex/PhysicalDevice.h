#pragma once

#include <Vex/RHIImpl/RHIPhysicalDevice.h>


#include <RHI/RHIFwd.h>

namespace vex
{

inline std::unique_ptr<RHIPhysicalDevice> GPhysicalDevice = nullptr;

} // namespace vex