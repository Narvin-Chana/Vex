cmake_minimum_required(VERSION 3.27)
project(Vex VERSION 0.0.2 LANGUAGES CXX C)
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

# Add the build examples only if we're directly building cmake from Vex (so not if Vex is setup as an actual dependency).
# Examples are used to develop Vex, they should not be used or included in projects that use Vex.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    option(VEX_BUILD_EXAMPLES "Build the example programs" ON)
    option(VEX_BUILD_TESTS "Build Vex tests" ON)
else()
    option(VEX_BUILD_EXAMPLES "Build the example programs" OFF)
    option(VEX_BUILD_TESTS "Build Vex tests" OFF)
endif()

# CMake Helpers
include(VexHelpers)

# Will correctly install our custom githook to run clang-format on staged files when committing
include(GitHooks)

# Contains all logic to fetch and include DXC
include(VexDXC)

if (VEX_BUILD_EXAMPLES OR VEX_BUILD_TESTS)
    option(VEX_ENABLE_SLANG "Enables the possibility to use the Slang shader compiler backend." ON)
else()
    option(VEX_ENABLE_SLANG "Enables the possibility to use the Slang shader compiler backend." OFF)
endif()

# Contains all logic to fetch and include Slang
include(VexSlang)

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
set(VEX_ENABLE_DX12 OFF CACHE INTERNAL "DirectX 12 backend enabled")
set(VEX_ENABLE_VULKAN OFF CACHE INTERNAL "Vulkan backend enabled")

if(VEX_SELECTED_BACKEND STREQUAL "DX12")
    set(VEX_ENABLE_DX12 ON CACHE INTERNAL "DirectX 12 backend enabled" FORCE)
elseif(VEX_SELECTED_BACKEND STREQUAL "VULKAN")
    set(VEX_ENABLE_VULKAN ON CACHE INTERNAL "Vulkan backend enabled" FORCE)
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
    # Platform
    "src/Vex/Platform/Platform.h"
    # RHI
    "src/RHI/RHIFwd.h"
    "src/RHI/RHI.h"  
    "src/RHI/RHICommandPool.h"
    "src/RHI/RHICommandPool.cpp"
    "src/RHI/RHISwapChain.h"
    "src/RHI/RHITexture.h"
    "src/RHI/RHIBuffer.h"
    "src/RHI/RHIBuffer.cpp"
    "src/RHI/RHIPipelineState.h"
    "src/RHI/RHIResourceLayout.cpp"
    "src/RHI/RHIBindings.h"
    "src/RHI/RHIBarrier.h"
    "src/RHI/RHIBarrier.cpp"
    "src/RHI/RHIResourceLayout.h"
    "src/RHI/RHIDescriptorPool.h"
    "src/RHI/RHIDescriptorPool.cpp"
    "src/RHI/RHICommandList.h"
    "src/RHI/RHICommandList.cpp"
    "src/RHI/RHIAllocator.h"
    "src/RHI/RHIAllocator.cpp"
    "src/RHI/RHIFence.h"
    "src/RHI/RHITimestampQueryPool.cpp"
    "src/RHI/RHITimestampQueryPool.h"
    "src/RHI/RHIScopedGPUEvent.h"
    "src/RHI/RHIScopedGPUEvent.cpp"
    "src/Vex/RHIImpl/RHIFence.h"
    "src/Vex/RHIImpl/RHICommandList.h"
    "src/Vex/RHIImpl/RHICommandPool.h"
    "src/Vex/RHIImpl/RHIDescriptorPool.h"
    "src/Vex/RHIImpl/RHIPipelineState.h"
    "src/Vex/RHIImpl/RHISwapChain.h"
    "src/Vex/RHIImpl/RHIResourceLayout.h"
    "src/Vex/RHIImpl/RHITexture.h"
    "src/Vex/RHIImpl/RHI.h"
    "src/Vex/RHIImpl/RHIBuffer.h"
    "src/Vex/RHIImpl/RHIAllocator.h"
    "src/Vex/RHIImpl/RHITimestampQueryPool.h"
    # Vex Utilities
    "src/Vex/Utility/UniqueHandle.h" 
    "src/Vex/Utility/Concepts.h"
    "src/Vex/Utility/EnumFlags.h"
    "src/Vex/Utility/Handle.h"
    "src/Vex/Utility/Formattable.h"    
    "src/Vex/Utility/Hash.h"
    "src/Vex/Utility/MaybeUninitialized.h"
    "src/Vex/Utility/NonNullPtr.h"
    "src/Vex/Utility/ByteUtils.h"
    "src/Vex/Utility/WString.h"
    "src/Vex/Utility/Visitor.h"
    # Vex Platform
    "src/Vex/Platform/PlatformWindow.h" 
    "src/Vex/Platform/Debug.h" 
    # Vex Containers
    "src/Vex/Containers/FreeList.h"
    "src/Vex/Containers/FreeList.cpp"
    "src/Vex/Containers/ResourceCleanup.h"
    "src/Vex/Containers/ResourceCleanup.cpp"
    "src/Vex/Containers/Span.h"
    # Vex Shader API
    "src/Vex/Shaders/ShaderKey.h"
    "src/Vex/Shaders/Shader.h"
    "src/Vex/Shaders/Shader.cpp"
    "src/Vex/Shaders/ShaderCompiler.h"
    "src/Vex/Shaders/ShaderCompiler.cpp"
    "src/Vex/Shaders/ShaderCompilerSettings.h"
    "src/Vex/Shaders/DXCImpl.h"
    "src/Vex/Shaders/DXCImpl.cpp"
    "src/Vex/Shaders/DXCImplReflection.cpp"
    "src/Vex/Shaders/ShaderEnvironment.h"
    "src/Vex/Shaders/CompilerBase.h"
    # Vex Built-In Shaders
    "src/Vex/BuiltInShaders/MipGeneration.h"
    # Vex API
    "src/Vex.h"
    "src/Vex/Graphics.h" 
    "src/Vex/Graphics.cpp" 
    "src/Vex/Formats.h" 
    "src/Vex/Types.h" 
    "src/Vex/Logger.h" 
    "src/Vex/Logger.cpp"
    "src/Vex/FeatureChecker.h" 
    "src/Vex/PhysicalDevice.h"
    "src/Vex/PhysicalDevice.cpp"
    "src/Vex/FrameResource.h"
    "src/Vex/Formats.cpp"
    "src/Vex/Texture.h"
    "src/Vex/Texture.cpp"
    "src/Vex/Buffer.h"
    "src/Vex/Buffer.cpp"
    "src/Vex/QueueType.h"
    "src/Vex/CommandContext.h"
    "src/Vex/CommandContext.cpp"
    "src/Vex/Bindings.h"
    "src/Vex/Bindings.cpp"
    "src/Vex/PipelineStateCache.h"
    "src/Vex/PipelineStateCache.cpp"
    "src/Vex/Resource.cpp"
    "src/Vex/Resource.h"
    "src/Vex/TextureSampler.h"
    "src/Vex/GraphicsPipeline.h"
    "src/Vex/DrawHelpers.h"
    "src/Vex/RenderExtension.h"
    "src/Vex/RenderExtension.cpp"
    "src/Vex/ResourceBindingUtils.cpp"
    "src/Vex/RayTracing.h"
    "src/Vex/RayTracing.cpp"
    "src/Vex/Synchronization.h"
    "src/Vex/ResourceCopy.h"
    "src/Vex/ResourceCopy.cpp"
    "src/Vex/ResourceReadbackContext.cpp"
    "src/Vex/ResourceReadbackContext.h"
)

if(WIN32)
    target_sources(Vex PRIVATE
        "src/Vex/Platform/Windows/HResult.h"
    )
endif()

if(LINUX)
    target_sources(Vex PRIVATE
    # Nothing for now...
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

# Static linking with DXC.
build_with_dxc(Vex)

# Static linking with Slang.
build_with_slang(Vex)

# Static linking to magic_enum
add_header_only_dependency(Vex magic_enum "${magic_enum_SOURCE_DIR}" "include" "magic_enum")

# =========================================
# DEFINES
# =========================================

# =========================================
# Set configuration defines
# Debug: Full debug symbols
# Development: Optimizations with some debug symbols
# Shipping: Fully optimized build without debug symbols
# =========================================
target_compile_definitions(Vex PUBLIC
    $<$<CONFIG:Debug>:VEX_DEBUG=1 VEX_DEVELOPMENT=0 VEX_SHIPPING=0>
    $<$<CONFIG:RelWithDebInfo>:VEX_DEBUG=0 VEX_DEVELOPMENT=1 VEX_SHIPPING=0>
    $<$<CONFIG:Release>:VEX_DEBUG=0 VEX_DEVELOPMENT=0 VEX_SHIPPING=1>
)

# Graphics API defines
target_compile_definitions(Vex PUBLIC VEX_DX12=$<BOOL:${VEX_ENABLE_DX12}> VEX_VULKAN=$<BOOL:${VEX_ENABLE_VULKAN}>)

if(WIN32)
    # Avoid windows polluting the code with macros.
    # Allow for typically "unsafe" C functions.
    target_compile_definitions(Vex PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

target_compile_definitions(Vex PUBLIC VEX_TESTS=$<BOOL:${VEX_BUILD_TESTS}>)

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

# Add the examples projects.
if(VEX_BUILD_EXAMPLES)
    message(STATUS "Building Vex examples...")
    add_subdirectory(examples)
endif()
# Add the tests project.
if (VEX_BUILD_TESTS)
    message(STATUS "Building Vex tests...")
    add_subdirectory(tests)
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

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VexHelpers.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/VexHelpers.cmake"
    COPYONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/VexConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/VexConfigVersion.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/VexHelpers.cmake"
    DESTINATION lib/cmake/Vex
)
