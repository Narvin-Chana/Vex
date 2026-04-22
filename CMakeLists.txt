cmake_minimum_required(VERSION 4.2.2)
include(CMakePackageConfigHelpers)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(Vex VERSION 0.0.5 LANGUAGES CXX C)

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
    option(VEX_BUILD_TOOLS "Build Vex tools" ON)
else()
    option(VEX_BUILD_EXAMPLES "Build the example programs" OFF)
    option(VEX_BUILD_TESTS "Build Vex tests" OFF)
    option(VEX_BUILD_TOOLS "Build Vex tools" OFF)
endif()

# Detect clang-cl (Clang frontend with MSVC ABI has no module scanning support)
# This will be fixed in CMake 4.4.0.
# See https://gitlab.kitware.com/cmake/cmake/-/merge_requests/11875
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(VEX_COMPILER_SUPPORTS_MODULES OFF)
    message(STATUS "Detected clang-cl: C++20 module scanning not supported, disabling modules. This will be fixed in CMake 4.4.0.")
else()
     set(VEX_COMPILER_SUPPORTS_MODULES ON)
endif()

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    option(VEX_USE_MODULES "Determines whether to generate and export the Vex cpp20 module." ${VEX_COMPILER_SUPPORTS_MODULES})
else()
    option(VEX_USE_MODULES "Determines whether to generate and export the Vex cpp20 module." OFF)
endif()

# CMake Helpers
include(VexHelpers)

# Will correctly install our custom githook to run clang-format on staged files when committing
include(GitHooks)

option(VEX_ENABLE_SHADER_COMPILER "Enables the possibility to use Vex's built-in ShaderCompiler." ON)
option(VEX_ENABLE_DXC "Enables the possibility to use the DXC shader compiler backend." ON)
if (VEX_BUILD_EXAMPLES OR VEX_BUILD_TESTS)
    option(VEX_ENABLE_SLANG "Enables the possibility to use the Slang shader compiler backend." ON)
else()
    option(VEX_ENABLE_SLANG "Enables the possibility to use the Slang shader compiler backend." OFF)
endif()

# Contains all logic to fetch and include shader compiler backends.
include(VexDXC)
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
        find_package(Vulkan 1.4 REQUIRED QUIET)
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
    find_package(Vulkan 1.4 REQUIRED QUIET)
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
    "src/RHI/RHIResourceLayout.h"
    "src/RHI/RHIDescriptorPool.h"
    "src/RHI/RHIDescriptorPool.cpp"
    "src/RHI/RHIAccelerationStructure.h"
    "src/RHI/RHIAccelerationStructure.cpp"
    "src/RHI/RHICommandList.h"
    "src/RHI/RHICommandList.cpp"
    "src/RHI/RHIAllocator.h"
    "src/RHI/RHIAllocator.cpp"
    "src/RHI/RHIFence.h"
    "src/RHI/RHITimestampQueryPool.cpp"
    "src/RHI/RHITimestampQueryPool.h"
    "src/RHI/RHIScopedGPUEvent.h"
    "src/RHI/RHIScopedGPUEvent.cpp"
    "src/RHI/RHIPhysicalDevice.h"
    "src/RHI/RHIPhysicalDevice.cpp"
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
    "src/Vex/RHIImpl/RHIAccelerationStructure.h"
    "src/Vex/RHIImpl/RHIPhysicalDevice.h"
    # Vex Utilities
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
    "src/Vex/Utility/Algorithms.h"
    # Vex Platform
    "src/Vex/Platform/PlatformWindow.h"
        # Vex Containers
    "src/Vex/Containers/FreeList.h"
    "src/Vex/Containers/Span.h"
    # Vex Built-In Shaders
    "src/Vex/Generated/MipGeneration.h"
    # Vex API
    "src/Vex.h"
    "src/Vex/Graphics.h"
    "src/Vex/Graphics.cpp"
    "src/Vex/Formats.h"
    "src/Vex/Types.h"
    "src/Vex/Logger.h"
    "src/Vex/Logger.cpp"
    "src/Vex/FrameResource.h"
    "src/Vex/Formats.cpp"
    "src/Vex/Texture.h"
    "src/Vex/Texture.cpp"
    "src/Vex/Buffer.h"
    "src/Vex/Buffer.cpp"
    "src/Vex/ResourceCleanup.h"
    "src/Vex/ResourceCleanup.cpp"
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
    "src/Vex/ResourceBindingUtils.cpp"
    "src/Vex/Synchronization.h"
    "src/Vex/ResourceCopy.h"
    "src/Vex/ResourceCopy.cpp"
    "src/Vex/ResourceReadbackContext.cpp"
    "src/Vex/ResourceReadbackContext.h"
    "src/Vex/AccelerationStructure.h"
    "src/Vex/PhysicalDevice.h"
    "src/Vex/TextureStateMap.h"
    "src/Vex/TextureStateMap.cpp"
    "src/Vex/Utility/SHA1.h"
    "src/Vex/Utility/SHA1.cpp"
    "src/Vex/BuildAccelerationStructure.h"
    "src/Vex/Utility/Functional/__functional_base.h"
    "src/Vex/Utility/Functional/function.h"
    "src/Vex/Utility/Functional/function_ref.h"
    "src/Vex/Utility/Functional/move_only_function.h"
    "src/Vex/ShaderView.h"
    "src/Vex/RayTracing.h"
    "src/Vex/PipelineState.cpp"
    "src/Vex/PipelineState.h"
        src/VexMacros.h
        src/Vex/Platform/Debug.h
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

# Vex Shader Compiler
if(VEX_ENABLE_SHADER_COMPILER)
    target_sources(Vex PRIVATE
        "src/ShaderCompiler/ShaderKey.h"
        "src/ShaderCompiler/Shader.h"
        "src/ShaderCompiler/Shader.cpp"
        "src/ShaderCompiler/ShaderCompiler.h"
        "src/ShaderCompiler/ShaderCompiler.cpp"
        "src/ShaderCompiler/ShaderCompilerSettings.h"
        "src/ShaderCompiler/Compiler/DXC/DXCCompiler.h"
        "src/ShaderCompiler/Compiler/DXC/DXCCompiler.cpp"
        "src/ShaderCompiler/Compiler/DXC/DXCReflection.cpp"
        "src/ShaderCompiler/ShaderEnvironment.h"
        "src/ShaderCompiler/Compiler/CompilerBase.h"
        "src/ShaderCompiler/RayTracingShaderKey.h"
        "src/ShaderCompiler/RayTracingShaderKey.cpp"
    )
endif()

if(VEX_USE_MODULES)
    target_sources(Vex PUBLIC FILE_SET CXX_MODULES FILES "src/Vex.cppm" "${magic_enum_SOURCE_DIR}/module/magic_enum.cppm")
endif()

set_target_properties(Vex PROPERTIES CXX_SCAN_FOR_MODULES ${VEX_USE_MODULES})

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

target_compile_definitions(Vex PUBLIC VEX_MODULES=$<BOOL:${VEX_USE_MODULES}>)

# Shader compiler defines
target_compile_definitions(Vex PUBLIC VEX_SHADER_COMPILER=$<BOOL:${VEX_ENABLE_SHADER_COMPILER}>)
target_compile_definitions(Vex PUBLIC VEX_DXC=$<BOOL:${VEX_ENABLE_DXC}>)
target_compile_definitions(Vex PUBLIC VEX_SLANG=$<BOOL:${VEX_ENABLE_SLANG}>)

# =========================================
# INSTALLATION
# =========================================
message(STATUS "Installing Vex...")
# Gets the list of registered header dependencies (previous sections)
get_property(HEADER_DEPS_TO_INSTALL GLOBAL PROPERTY HEADER_DEPS_TO_INSTALL)

# Installation rules
if(VEX_USE_MODULES)
    install(TARGETS Vex ${HEADER_DEPS_TO_INSTALL}
            EXPORT VexTargets
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib
            RUNTIME DESTINATION bin
            FILE_SET CXX_MODULES DESTINATION modules
    )
else()
    install(TARGETS Vex ${HEADER_DEPS_TO_INSTALL}
            EXPORT VexTargets
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib
            RUNTIME DESTINATION bin
    )
endif()

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
# Add the tools projects.
if (VEX_BUILD_TOOLS)
    message(STATUS "Building Vex tools...")
    add_subdirectory(tools/GenerateBuiltInShaders)
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
