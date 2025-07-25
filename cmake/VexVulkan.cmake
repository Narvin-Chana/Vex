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

    message(STATUS "Vulkan_FOUND: ${Vulkan_FOUND}")
    message(STATUS "Vulkan_LIBRARIES: ${Vulkan_LIBRARIES}")
    message(STATUS "Vulkan_INCLUDE_DIRS: ${Vulkan_INCLUDE_DIRS}")
    message(STATUS "Vulkan library path: ${Vulkan_LIBRARY}")

    # Vulkan source files
    set(VEX_VULKAN_SOURCES
        "src/Vulkan/VkFormats.h" 
        "src/Vulkan/VkFeatureChecker.h"
        "src/Vulkan/VkFeatureChecker.cpp"
        "src/Vulkan/VkRHI.h"
        "src/Vulkan/VkRHI.cpp"
        "src/Vulkan/VkExtensions.cpp"
        "src/Vulkan/VkDebug.h"
        "src/Vulkan/VkDebug.cpp"
        "src/Vulkan/VkErrorHandler.h"
        "src/Vulkan/VkErrorHandler.cpp"
        "src/Vulkan/VkPhysicalDevice.h"
        "src/Vulkan/VkPhysicalDevice.cpp"
        "src/Vulkan/VkCommandPool.cpp"
        "src/Vulkan/VkCommandPool.h"
        "src/Vulkan/VkCommandQueue.cpp"
        "src/Vulkan/VkCommandQueue.h"
        "src/Vulkan/VkCommandList.cpp"
        "src/Vulkan/VkCommandList.h"
        "src/Vulkan/Synchro/VkFence.cpp"
        "src/Vulkan/Synchro/VkFence.h"
        "src/Vulkan/VkSwapChain.h"
        "src/Vulkan/VkSwapChain.cpp"
        "src/Vulkan/VkShader.h"
        "src/Vulkan/VkShader.cpp"
        "src/Vulkan/VkPipelineState.h"
        "src/Vulkan/VkPipelineState.cpp"
        "src/Vulkan/VkResourceLayout.h"
        "src/Vulkan/VkResourceLayout.cpp"
        "src/Vulkan/VkTexture.h"
        "src/Vulkan/VkTexture.cpp"
        "src/Vulkan/VkGPUContext.h"
        "src/Vulkan/VkMemory.h"
        "src/Vulkan/VkDescriptorPool.h"
        "src/Vulkan/VkDescriptorPool.cpp"
    )

    # Add Vulkan sources to target
    target_sources(${TARGET} PRIVATE ${VEX_VULKAN_SOURCES})

    # Link Vulkan libraries
    target_link_libraries(${TARGET} PUBLIC Vulkan::Vulkan)

    # Make Vulkan-Hpp headers available
    add_header_only_dependency(${TARGET} vulkanhpp "${vulkanhpp_SOURCE_DIR}" "vulkan" "vulkan")

    message(STATUS "Vulkan backend configured successfully")
endfunction()