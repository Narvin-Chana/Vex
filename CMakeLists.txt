cmake_minimum_required(VERSION 3.27)
project(Vex VERSION 0.0.1 LANGUAGES CXX)
include(CMakePackageConfigHelpers)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Store the root directory of Vex
set(VEX_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
# Make VEX_ROOT_DIR available to downstream projects that use this library
set(VEX_ROOT_DIR "${VEX_ROOT_DIR}" CACHE INTERNAL "Path to Vex's library root directory")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# CMake Helpers
include(VexHelpers)

# Will correctly install our custom githook to run clang-format on staged files when committing
include(GitHooks)

# Contains all logic to fetch and include DXC
include(VexDXC)

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
# Graphics Backend Selection
# =========================================
message(STATUS "Configuring graphics backend...")

# Define backend options
set(VEX_GRAPHICS_BACKEND "AUTO" CACHE STRING "Graphics backend to use (AUTO, DX12, VULKAN)")
set_property(CACHE VEX_GRAPHICS_BACKEND PROPERTY STRINGS "AUTO" "DX12" "VULKAN")

# Determine which backend to use
set(VEX_SELECTED_BACKEND "")

if(VEX_GRAPHICS_BACKEND STREQUAL "AUTO")
    # Auto-detect best available backend
    if(WIN32)
        set(VEX_SELECTED_BACKEND "DX12")
        message(STATUS "Auto-selected DirectX 12 backend for Windows")
    else()
        find_package(Vulkan QUIET)
        if(Vulkan_FOUND)
            set(VEX_SELECTED_BACKEND "VULKAN")
            message(STATUS "Auto-selected Vulkan backend")
        else()
            message(FATAL_ERROR "No suitable graphics backend found. Vulkan SDK is required on non-Windows platforms.")
        endif()
    endif()
elseif(VEX_GRAPHICS_BACKEND STREQUAL "DX12")
    if(NOT WIN32)
        message(FATAL_ERROR "DirectX 12 backend is only available on Windows")
    endif()
    set(VEX_SELECTED_BACKEND "DX12")
    message(STATUS "Explicitly selected DirectX 12 backend")
elseif(VEX_GRAPHICS_BACKEND STREQUAL "VULKAN")
    find_package(Vulkan QUIET)
    if(NOT Vulkan_FOUND)
        message(FATAL_ERROR "Vulkan SDK not found, cannot use Vulkan backend")
    endif()
    set(VEX_SELECTED_BACKEND "VULKAN")
    message(STATUS "Explicitly selected Vulkan backend")
else()
    message(FATAL_ERROR "Invalid VEX_GRAPHICS_BACKEND: ${VEX_GRAPHICS_BACKEND}. Must be AUTO, DX12, or VULKAN")
endif()

# Set backend flags
set(VEX_ENABLE_DX12 OFF)
set(VEX_ENABLE_VULKAN OFF)

if(VEX_SELECTED_BACKEND STREQUAL "DX12")
    set(VEX_ENABLE_DX12 ON)
    add_compile_definitions(NOMINMAX)
elseif(VEX_SELECTED_BACKEND STREQUAL "VULKAN")
    set(VEX_ENABLE_VULKAN ON)
else()
    message(FATAL_ERROR "No graphics backend was selected, please check your configuration.")
endif()

message(STATUS "Selected graphics backend: ${VEX_SELECTED_BACKEND}")

# =========================================
# DEPENDENCIES FETCHING
# =========================================
# Dependencies fetching uses the FetchContent module
include(FetchContent)

# Fetch magic_enum dependency
message(STATUS "Fetching magic_enum...")
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG v0.9.7
)
FetchContent_MakeAvailable(magic_enum)
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
    "src/Vex/RHI/RHISwapChain.h"
    "src/Vex/RHI/RHITexture.h"
    "src/Vex/Formats.cpp"
    "src/Vex/RHI/RHIBuffer.h"
    "src/Vex/Texture.h"
    "src/Vex/Texture.cpp"
    "src/Vex/Buffer.h"
    "src/Vex/CommandQueueType.h"
    "src/Vex/RHI/RHIFwd.h"
    "src/Vex/CommandContext.h"
    "src/Vex/CommandContext.cpp"
    "src/Vex/ShaderKey.h"
    "src/Vex/Bindings.h"
    "src/Vex/Bindings.cpp"
    "src/Vex/RHI/RHIPipelineState.h"
    "src/Vex/PipelineStateCache.h"
    "src/Vex/PipelineStateCache.cpp"
    "src/Vex/RHI/RHIShader.h"
    "src/Vex/Hash.h"
    "src/Vex/RHI/RHIResourceLayout.h"
    "src/Vex/RHI/RHIResourceLayout.cpp"
    "src/Vex/Containers/FreeList.h"
    "src/Vex/Handle.h"
    "src/Vex/RHI/RHIDescriptorPool.h"
    "src/Vex/Resource.h"
    "src/Vex/RHI/RHIBindings.h"
    "src/Vex/Containers/ResourceCleanup.h"
    "src/Vex/Containers/ResourceCleanup.cpp"
    "src/Vex/ShaderCompiler.h"
    "src/Vex/ShaderCompiler.cpp"
    "src/Vex/Platform/Platform.h"
    "src/Vex/Formattable.h"
    "src/Vex/ShaderResourceContext.h"
    "src/Vex/ShaderResourceContext.cpp"
    "src/Vex/ShaderGen.h"
    "src/Vex/GraphicsPipeline.h"
    "src/Vex/DrawHelpers.h"
)

if (WIN32)
    target_sources(Vex PRIVATE
        "src/Vex/Platform/Windows/WString.h"
        "src/Vex/Platform/Windows/HResult.h"
    )
endif()

if (LINUX)
    target_sources(Vex PRIVATE
        "src/Vex/Platform/Linux/WString.h"
    )
endif()

target_include_directories(Vex PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# =========================================
# REGISTER DEPENDENCIES
# =========================================
message(STATUS "Setting up Vex headers...")

# Configure selected backend
if(VEX_ENABLE_DX12)
    include(VexDX12)
    setup_dx12_backend(Vex)
elseif(VEX_ENABLE_VULKAN)  
    include(VexVulkan)
    setup_vulkan_backend(Vex)
endif()

function(vex_setup_runtime TARGET)
    if(VEX_ENABLE_DX12)
        include(VexDX12)
        vex_setup_d3d12_agility_runtime(${TARGET})
    endif()
endfunction()

# Static linking with DXC.
build_with_dxc(Vex)

# Static linking to magic_enum
add_header_only_dependency(Vex magic_enum "${magic_enum_SOURCE_DIR}" "include" "magic_enum")

# =========================================
# DEFINES
# =========================================

# Graphics API defines
target_compile_definitions(Vex PUBLIC VEX_DX12=$<BOOL:${VEX_ENABLE_DX12}> VEX_VULKAN=$<BOOL:${VEX_ENABLE_VULKAN}>)

if(WIN32)
    # Avoid windows polluting the code with macros.
    target_compile_definitions(Vex PUBLIC NOMINMAX WIN32_MEAN_AND_LEAN)
endif()

# =========================================
# INSTALLATION
# =========================================
message(STATUS "Installing Vex...")
# Gets the list of registered header dependencies (previous sections)
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
