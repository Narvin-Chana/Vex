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
        "src/Vulkan/RHI/VkTexture.h"
        "src/Vulkan/RHI/VkTexture.cpp"
        "src/Vulkan/RHI/VkBuffer.h"
        "src/Vulkan/RHI/VkBuffer.cpp"
        "src/Vulkan/RHI/VkGraphicsPipeline.h"
        "src/Vulkan/RHI/VkGraphicsPipeline.cpp"
        # Vulkan API
        "src/Vulkan/VkFormats.h" 
        "src/Vulkan/VkFeatureChecker.h"
        "src/Vulkan/VkFeatureChecker.cpp"
        "src/Vulkan/VkExtensions.cpp"
        "src/Vulkan/VkDebug.h"
        "src/Vulkan/VkDebug.cpp"
        "src/Vulkan/VkErrorHandler.h"
        "src/Vulkan/VkPhysicalDevice.h"
        "src/Vulkan/VkPhysicalDevice.cpp"
        "src/Vulkan/VkCommandQueue.h"
        "src/Vulkan/VkGPUContext.h"
        "src/Vulkan/VkMemory.h"
    )

    # Add Vulkan sources to target
    target_sources(${TARGET} PRIVATE ${VEX_VULKAN_SOURCES})

    # Link Vulkan libraries
    target_link_libraries(${TARGET} PUBLIC Vulkan::Vulkan)

    # Make Vulkan-Hpp headers available
    add_header_only_dependency(${TARGET} vulkanhpp "${vulkanhpp_SOURCE_DIR}" "vulkan" "vulkan")

    message(STATUS "Vulkan backend configured successfully")
endfunction()