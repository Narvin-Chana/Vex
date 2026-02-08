#pragma once

#include <Vex/RHIImpl/RHIPhysicalDevice.h>
#include <Vex/Utility/UniqueHandle.h>

#include <RHI/RHIFwd.h>

namespace vex
{

inline UniqueHandle<RHIPhysicalDevice> GPhysicalDevice = nullptr;

} // namespace vex