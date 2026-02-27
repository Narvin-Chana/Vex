# VexVulkan.cmake - Vulkan backend configuration

function(setup_vulkan_backend TARGET)
    message(STATUS "Setting up Vulkan backend...")

    # Fetch Vulkan-Hpp dependency
    message(STATUS "Fetching VulkanHpp...")
    FetchContent_Declare(
        VulkanHpp
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Hpp.git
        GIT_TAG        v1.4.323
    )
    FetchContent_MakeAvailable(VulkanHpp)

    # Fetch SPIRV-Reflect dependency
    message(STATUS "Fetching SPIRV-Reflect...")
    FetchContent_Declare(
            spirv_reflect
            SOURCE_SUBDIR  "<invalid path>" # this forces FetchContent to pull but not configure the dependency
            GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
            GIT_TAG        vulkan-sdk-1.4.321.0
    )
    FetchContent_MakeAvailable(spirv_reflect)

    add_library(vex-spirv-reflect OBJECT
        ${spirv_reflect_SOURCE_DIR}/spirv_reflect.c)

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
    )

    # Add Vulkan sources to target
    target_sources(${TARGET} PRIVATE ${VEX_VULKAN_SOURCES})

    # Link Vulkan libraries
    target_link_libraries(${TARGET} PUBLIC Vulkan::Vulkan vex-spirv-reflect)

    # Make Vulkan-Hpp headers available
    add_header_only_dependency(${TARGET} vulkanhpp "${vulkanhpp_SOURCE_DIR}" "vulkan" "vulkan")

    target_include_directories(${TARGET} PUBLIC
        $<BUILD_INTERFACE:${spirv_reflect_SOURCE_DIR}>
        $<INSTALL_INTERFACE:>
    )

    set_property(GLOBAL APPEND PROPERTY HEADER_DEPS_TO_INSTALL vex-spirv-reflect)

    message(STATUS "Vulkan backend configured successfully")
endfunction()