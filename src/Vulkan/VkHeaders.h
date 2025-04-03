#pragma once
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS
#define VK_NO_PROTOTYPES

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(linux)
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#include <vulkan/vulkan.hpp>
