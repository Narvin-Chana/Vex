# VexVulkan.cmake - Vulkan backend configuration
#
# Dependencies, see the "Dependencies" section of the README for the CMake variables controlling them:
#  - The Vulkan SDK (Vulkan::Vulkan), which must be installed.
#  - SPIRV-Reflect, provides unofficial::spirv-reflect (the target name of vcpkg's spirv-reflect port). An installed
#    unofficial-spirv-reflect package (at least SPIRV_REFLECT_VERSION) is used when found, otherwise the pinned
#    release is downloaded and its single source file is compiled into Vex.
include(FetchContent)

# 1.4.321 is the minimum version required due to the use of VK_KHR_unified_image_layouts which was added in that version.
# If you change this version, you should also update the version used in "cmake-multi-platform.yml".
set(VULKAN_MIN_VERSION 1.4.321)
find_package(Vulkan ${VULKAN_MIN_VERSION} REQUIRED)

set(SPIRV_REFLECT_VERSION 1.4.321.0)
FetchContent_Declare(spirv_reflect
    URL "https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-${SPIRV_REFLECT_VERSION}.tar.gz"
    URL_HASH SHA256=254ed36128e7abe8fb12ef804319d0790e059093c116e9fe55d4003880515f85
    SOURCE_SUBDIR "<invalid path>" # SPIRV-Reflect is meant to be embedded, only its sources are needed.
    FIND_PACKAGE_ARGS ${SPIRV_REFLECT_VERSION} NAMES unofficial-spirv-reflect CONFIG GLOBAL
)
FetchContent_MakeAvailable(spirv_reflect)

function(setup_vulkan_backend TARGET)
    message(STATUS "Setting up Vulkan backend...")

    # Vulkan source files
    set(VEX_VULKAN_SOURCES
        # Vulkan RHI
        "src/Vulkan/VkRHIFwd.h"
        "src/Vulkan/RHI/VkRHI.h"
        "src/Vulkan/RHI/VkRHI.cpp"
        "src/Vulkan/RHI/VkCommandPool.cpp"
        "src/Vulkan/RHI/VkCommandPool.h"
        "src/Vulkan/RHI/VkCommandList.cpp"
        "src/Vulkan/RHI/VkCommandList.h"
        "src/Vulkan/RHI/VkSwapChain.h"
        "src/Vulkan/RHI/VkSwapChain.cpp"
        "src/Vulkan/RHI/VkPipelineState.h"
        "src/Vulkan/RHI/VkPipelineState.cpp"
        "src/Vulkan/RHI/VkResourceLayout.h"
        "src/Vulkan/RHI/VkResourceLayout.cpp"
        "src/Vulkan/RHI/VkDescriptorPool.h"
        "src/Vulkan/RHI/VkDescriptorPool.cpp"
        "src/Vulkan/RHI/VkDescriptorSet.h"
        "src/Vulkan/RHI/VkDescriptorSet.cpp"
        "src/Vulkan/RHI/VkTexture.h"
        "src/Vulkan/RHI/VkTexture.cpp"
        "src/Vulkan/RHI/VkBarrier.h"
        "src/Vulkan/RHI/VkBarrier.cpp"
        "src/Vulkan/RHI/VkBuffer.h"
        "src/Vulkan/RHI/VkBuffer.cpp"
        "src/Vulkan/RHI/VkAllocator.h"
        "src/Vulkan/RHI/VkAllocator.cpp"
        "src/Vulkan/RHI/VkFence.h"
        "src/Vulkan/RHI/VkFence.cpp"
        "src/Vulkan/RHI/VkTimestampQueryPool.cpp"
        "src/Vulkan/RHI/VkTimestampQueryPool.h"
        "src/Vulkan/RHI/VkScopedGPUEvent.h"
        "src/Vulkan/RHI/VkScopedGPUEvent.cpp"
        "src/Vulkan/RHI/VkAccelerationStructure.h"
        "src/Vulkan/RHI/VkAccelerationStructure.cpp"
        "src/Vulkan/RHI/VkPhysicalDevice.h"
        "src/Vulkan/RHI/VkPhysicalDevice.cpp"
        "src/Vulkan/RHI/VkShaderTable.cpp"
        "src/Vulkan/RHI/VkShaderTable.h"
        # Vulkan API
        "src/Vulkan/VkGraphicsPipeline.h"
        "src/Vulkan/VkGraphicsPipeline.cpp"
        "src/Vulkan/VkFormats.h"
        "src/Vulkan/VkExtensions.cpp"
        "src/Vulkan/VkDebug.h"
        "src/Vulkan/VkDebug.cpp"
        "src/Vulkan/VkErrorHandler.h"
        "src/Vulkan/VkCommandQueue.h"
        "src/Vulkan/VkGPUContext.h"
        "src/Vulkan/VkSampler.h"
        "src/Vulkan/VkSampler.cpp"
    )

    # Add Vulkan sources to target
    target_sources(${TARGET} PRIVATE ${VEX_VULKAN_SOURCES})

    target_link_libraries(${TARGET} PUBLIC Vulkan::Vulkan)
    if(TARGET unofficial::spirv-reflect)
        target_link_libraries(${TARGET} PRIVATE unofficial::spirv-reflect)
    else()
        # Not found as a package: the downloaded source file is compiled into Vex, consumers of Vex don't need it.
        target_sources(${TARGET} PRIVATE "${spirv_reflect_SOURCE_DIR}/spirv_reflect.c")
        target_include_directories(${TARGET} PRIVATE "$<BUILD_INTERFACE:${spirv_reflect_SOURCE_DIR}>")
    endif()

    message(STATUS "Vulkan backend configured successfully")
endfunction()