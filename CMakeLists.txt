cmake_minimum_required(VERSION 3.27)
project(Vex VERSION 0.0.1 LANGUAGES CXX)
include(CMakePackageConfigHelpers)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Will correctly install our custom githook to run clang-format on staged files when committing
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitHooks.cmake)

# Store the root directory of Vex
set(VEX_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
# Make VEX_ROOT_DIR available to downstream projects that use this library
set(VEX_ROOT_DIR "${VEX_ROOT_DIR}" CACHE INTERNAL "Path to Vex's library root directory")

# =========================================
# Set configuration flags
# Debug: Full debug symbols
# Development: Optimizations with some debug symbols
# Shipping: Fully optimized build without debug symbols
# =========================================
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DVEX_DEBUG=1 -DVEX_DEVELOPMENT=0 -DVEX_SHIPPING=0")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DVEX_DEBUG=0 -DVEX_DEVELOPMENT=1 -DVEX_SHIPPING=0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DVEX_DEBUG=0 -DVEX_DEVELOPMENT=0 -DVEX_SHIPPING=1")

# =========================================
# Check for available graphics APIs
# =========================================
message(STATUS "Checking for available graphics APIs...")
set(VEX_AVAILABLE_BACKENDS "")

# Check for DirectX 12 (Windows only)
if(WIN32)
    option(VEX_ENABLE_DX12 "Enable DirectX 12 backend if available" ON)
    if(VEX_ENABLE_DX12)
        list(APPEND VEX_AVAILABLE_BACKENDS "DirectX12")
    endif()
endif()

# Check for Vulkan
option(VEX_ENABLE_VULKAN "Enable Vulkan backend if available" ON)
if(VEX_ENABLE_VULKAN)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        list(APPEND VEX_AVAILABLE_BACKENDS "Vulkan")
    else()
        message(STATUS "Vulkan SDK not found, disabling Vulkan backend")
        set(VEX_ENABLE_VULKAN OFF)
    endif()
endif()

# Report available backends
message(STATUS "Available graphics backends: ${VEX_AVAILABLE_BACKENDS}")

# Make sure we have at least one backend
list(LENGTH VEX_AVAILABLE_BACKENDS NUM_BACKENDS)
if(NUM_BACKENDS EQUAL 0)
    message(ERROR "No graphics backends available. Some functionality may be limited.")
endif()

# =========================================
# DEPENDENCIES FETCHING
# =========================================
# Dependencies fetching uses the FetchContent module
include(FetchContent)

# Fetch efsw dependency
message(STATUS "Fetching efsw...")
FetchContent_Declare(
    efsw
    GIT_REPOSITORY https://github.com/SpartanJ/efsw.git
    GIT_TAG        1.4.1
)
# Completely override efsw's build options
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)  # Enable option() to override cached variables
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Force static libraries" FORCE)
set(EFSW_SHARED OFF CACHE BOOL "Disable efsw shared library" FORCE)
set(EFSW_STATIC ON CACHE BOOL "Enable efsw static library" FORCE)

# Prevent any additional targets
set(EFSW_BUILD_TEST OFF CACHE BOOL "Disable efsw tests" FORCE)
set(EFSW_BUILD_SAMPLES OFF CACHE BOOL "Disable efsw samples" FORCE)

FetchContent_MakeAvailable(efsw)

# Fetch Vulkan-Hpp dependency
message(STATUS "Fetching VulkanHpp...")
include(FetchContent)
FetchContent_Declare(
    VulkanHpp
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Hpp.git
    GIT_TAG        v1.4.309
)
FetchContent_MakeAvailable(VulkanHpp)

# Fetch magic_enum dependency
message(STATUS "Fetching magic_enum...")
include(FetchContent)
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG v0.9.7
)
FetchContent_MakeAvailable(magic_enum)

# Fetch DX12 Agility SDK
if (WIN32 AND VEX_ENABLE_DX12)
    message(STATUS "Fetching DX12 Agility SDK...")
    set(DX_AGILITY_SDK_VERSION "615")
    FetchContent_Declare(
        DirectX-Headers
        GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
        GIT_TAG v1.${DX_AGILITY_SDK_VERSION}.0
    )
    FetchContent_MakeAvailable(DirectX-Headers)
endif()
# =========================================

# Set static library output
add_library(Vex STATIC     
    "src/Vex.h"
    "src/Vex.cpp" 
    "src/Vex/GfxBackend.h" 
    "src/Vex/GfxBackend.cpp" 
    "src/Vex/Formats.h" 
    "src/Vex/PlatformWindow.h" 
    "src/Vex/Types.h" 
    "src/Vex/UniqueHandle.h" 
    "src/Vex/Logger.h" 
    "src/Vex/Logger.cpp"
    "src/Vex/Debug.h" 
    "src/Vex/FeatureChecker.h" 
    "src/Vex/RHI/RHI.h"  
    "src/Vex/PhysicalDevice.h"
    "src/Vex/PhysicalDevice.cpp"
    "src/Vex/EnumFlags.h"
    "src/Vex/RHI/RHICommandList.h"
    "src/Vex/RHI/RHICommandPool.h"
    "src/Vex/RHI/RHIFence.h"
    "src/Vex/RHI/RHIFence.cpp"
    "src/Vex/FrameResource.h"
)

if (WIN32)
    target_sources(Vex PRIVATE
        "src/Vex/Platform/Windows/WString.h"
        "src/Vex/Platform/Windows/HResult.h"
    )
endif()

# TODO: add linux platform sources

target_include_directories(Vex PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# DX12 project files
if (WIN32 AND VEX_ENABLE_DX12)
    set(VEX_DX12_SOURCES 
        "src/DX12/DX12Headers.h"
        "src/DX12/DX12Formats.h" 
        "src/DX12/DX12FeatureChecker.h"
        "src/DX12/DX12FeatureChecker.cpp"
        "src/DX12/DX12RHI.h"
        "src/DX12/DX12RHI.cpp"
        "src/DX12/HRChecker.h"
        "src/DX12/HRChecker.cpp"
        "src/DX12/DXGIFactory.h"
        "src/DX12/DXGIFactory.cpp"
        "src/DX12/DX12PhysicalDevice.h"
        "src/DX12/DX12PhysicalDevice.cpp"
        "src/DX12/DX12Debug.h"
        "src/DX12/DX12Debug.cpp"
        "src/DX12/DX12Fence.h"
        "src/DX12/DX12Fence.cpp"
        "src/DX12/DX12CommandPool.h"
     "src/DX12/DX12CommandPool.cpp" "src/DX12/DX12CommandList.h" "src/DX12/DX12CommandList.cpp")
endif()

# Vulkan project files
if (VEX_ENABLE_VULKAN AND Vulkan_FOUND)
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
    )
endif()

# =========================================
# REGISTER DEPENDENCIES
# =========================================
message(STATUS "Setting up Vex headers...")

# Function to create and link header-only libraries
function(add_header_only_dependency TARGET DEP_NAME SOURCE_DIR INCLUDE_PATH INSTALL_PATH)
    # Create interface library for the headers
    add_library(${TARGET}_${DEP_NAME}_headers INTERFACE)
    
    # Set include directories
    target_include_directories(${TARGET}_${DEP_NAME}_headers INTERFACE
        $<BUILD_INTERFACE:${SOURCE_DIR}/${INCLUDE_PATH}>
        $<INSTALL_INTERFACE:include/${INSTALL_PATH}>
    )
    
    # Link to main target
    target_link_libraries(${TARGET} PUBLIC ${TARGET}_${DEP_NAME}_headers)
    
    # Add to the list of targets to install
    set_property(GLOBAL APPEND PROPERTY HEADER_DEPS_TO_INSTALL ${TARGET}_${DEP_NAME}_headers)
    
    # Register header install path for later
    set_property(GLOBAL APPEND PROPERTY HEADER_DIRS_TO_INSTALL 
                "${SOURCE_DIR}/${INCLUDE_PATH}" "include/${INSTALL_PATH}")
endfunction()

# Static linking to efsw
add_header_only_dependency(Vex efsw "${efsw_SOURCE_DIR}" "include" "efsw")

# Static linking to DirectX 12
if(WIN32 AND VEX_ENABLE_DX12)
    message(STATUS "DX12 found!")

    # DirectX Agility SDK headers
    add_header_only_dependency(Vex DirectX-Headers "${DirectX-Headers_SOURCE_DIR}" "include" "directx")

    target_link_libraries(Vex PUBLIC d3d12.lib dxgi.lib)
    target_sources(Vex PRIVATE ${VEX_DX12_SOURCES})
    # Vex DX12 definition
    target_compile_definitions(Vex PUBLIC VEX_DX12=1)
else()
    target_compile_definitions(Vex PUBLIC VEX_DX12=0)
endif()

function(vex_setup_d3d12_agility_runtime TARGET)
    if (WIN32 AND VEX_ENABLE_DX12)
        set(VEX_D3D12_AGILITY_SOURCE_SDK_DIR "${VEX_ROOT_DIR}/external/d3d12-agility-sdk/bin")

        if(NOT EXISTS "${VEX_D3D12_AGILITY_SOURCE_SDK_DIR}/D3D12Core.dll")
            message(ERROR "Missing D3D12Core.dll in Agility SDK directory: ${VEX_D3D12_AGILITY_SOURCE_SDK_DIR}.")
            return()
        endif()

        target_compile_definitions(Vex PRIVATE 
            DIRECTX_AGILITY_SDK_VERSION=${DX_AGILITY_SDK_VERSION}
            D3D12_AGILITY_SDK_ENABLED
        )

        target_compile_definitions(${TARGET} PRIVATE 
            DIRECTX_AGILITY_SDK_VERSION=${DX_AGILITY_SDK_VERSION}
            D3D12_AGILITY_SDK_ENABLED
        )

        target_sources(${TARGET} PRIVATE
            "${VEX_ROOT_DIR}/src/DX12/DX12AgilitySDK.cpp"
        )

        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET}>/D3D12"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VEX_D3D12_AGILITY_SOURCE_SDK_DIR}/D3D12Core.dll"
                "${VEX_D3D12_AGILITY_SOURCE_SDK_DIR}/d3d12SDKLayers.dll"
                "$<TARGET_FILE_DIR:${TARGET}>/D3D12"
            COMMENT "Copying Agility SDK's DLLs to output directory : $<TARGET_FILE_DIR:${TARGET}>/D3D12..."
        )

        message(STATUS "D3D12 Agility SDK will be deployed with ${TARGET}")
    endif()
endfunction()

# Static linking to Vulkan
if(VEX_ENABLE_VULKAN AND Vulkan_FOUND)
    message(STATUS "Vulkan found!")
    target_sources(Vex PRIVATE ${VEX_VULKAN_SOURCES})
    target_link_libraries(Vex PUBLIC Vulkan::Vulkan)
    # Make Vulkan-Hpp headers available
    add_header_only_dependency(Vex vulkanhpp "${vulkanhpp_SOURCE_DIR}" "include" "vulkan")
    target_compile_definitions(Vex PUBLIC VEX_VULKAN=1)
else()
    target_compile_definitions(Vex PUBLIC VEX_VULKAN=0)
endif()

# Static linking to magic_enum
add_header_only_dependency(Vex magic_enum "${magic_enum_SOURCE_DIR}" "include" "magic_enum")

# =========================================
# INSTALLATION
# =========================================
message(STATUS "Installing Vex...")
# Gets the list of registered header dependencies (previous section)
get_property(HEADER_DEPS_TO_INSTALL GLOBAL PROPERTY HEADER_DEPS_TO_INSTALL)

# Installation rules
install(TARGETS Vex ${HEADER_DEPS_TO_INSTALL}
    EXPORT VexTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install Vex headers
install(DIRECTORY src/ DESTINATION include FILES_MATCHING PATTERN "*.h")

# Install the headers of our dependencies
get_property(HEADER_DIRS_TO_INSTALL GLOBAL PROPERTY HEADER_DIRS_TO_INSTALL)
list(LENGTH HEADER_DIRS_TO_INSTALL NUM_ITEMS)
math(EXPR NUM_PAIRS "${NUM_ITEMS} / 2")
foreach(INDEX RANGE ${NUM_PAIRS})
    if(INDEX LESS ${NUM_PAIRS})
        math(EXPR SRC_INDEX "${INDEX} * 2")
        math(EXPR DST_INDEX "${SRC_INDEX} + 1")
        list(GET HEADER_DIRS_TO_INSTALL ${SRC_INDEX} SRC_DIR)
        list(GET HEADER_DIRS_TO_INSTALL ${DST_INDEX} DST_DIR)
        if(EXISTS "${SRC_DIR}")
            install(DIRECTORY "${SRC_DIR}/" DESTINATION "${DST_DIR}" FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
        endif()
    endif()
endforeach()

# Add the build examples only if we're directly building cmake from Vex (so not if Vex is setup as an actual dependency).
# Examples are used to develop Vex, they should not be used or included in projects that use Vex.
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    option(VEX_BUILD_EXAMPLES "Build the example programs" ON)
else()
    option(VEX_BUILD_EXAMPLES "Build the example programs" OFF)
endif()
if (VEX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(EXPORT VexTargets
    FILE VexTargets.cmake
    NAMESPACE Vex::
    DESTINATION lib/cmake/Vex
)

# Config file for find_package()
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/VexConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VexConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/VexConfig.cmake"
    INSTALL_DESTINATION lib/cmake/Vex
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/VexConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/VexConfigVersion.cmake"
    DESTINATION lib/cmake/Vex
)
