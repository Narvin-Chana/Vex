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

# Examples, tests and tools are used to develop Vex, they should not be used or included in projects that use Vex (to enable them modify your CMakePresets.json).
option(VEX_BUILD_EXAMPLES "Build the example programs" OFF)
option(VEX_BUILD_TESTS "Build Vex tests" OFF)
option(VEX_BUILD_TOOLS "Build Vex tools" OFF)

# Detect clang-cl (Clang frontend with MSVC ABI has no module scanning support)
# This is fixed in CMake 4.4.0.
# See https://gitlab.kitware.com/cmake/cmake/-/merge_requests/11875
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC" AND CMAKE_VERSION VERSION_LESS "4.4")
    set(VEX_COMPILER_SUPPORTS_MODULES OFF)
    message(STATUS "Detected clang-cl: C++20 module scanning not supported, disabling modules. This will be fixed in CMake 4.4.0.")
else()
     set(VEX_COMPILER_SUPPORTS_MODULES ON)
endif()

# Side-note regarding CMake modules support, if you wish to use "import std;" in your project, you must set the CMAKE_EXPERIMENTAL_CXX_IMPORT_STD variable to the value indicated here:
# https://github.com/Kitware/CMake/blob/v4.4.0-rc2/Help/dev/experimental.rst
# This feature flag depends on your CMake version, so make sure to select the appropriate branch.
option(VEX_USE_MODULES "Determines whether to generate and export the Vex cpp20 module (experimental due to flaky compiler/build-system support)." OFF)

# Modules could be unsupported with the current compiler.
set(VEX_INTERNAL_USE_MODULES OFF)
if(VEX_COMPILER_SUPPORTS_MODULES AND VEX_USE_MODULES)
    set(VEX_INTERNAL_USE_MODULES ON)
endif()

# CMake Helpers
include(VexHelpers)

# Will correctly install our custom githook to run clang-format on staged files when committing
include(GitHooks)

set(VEX_ENABLE_SLANG_DEFAULT OFF)
if(VEX_BUILD_EXAMPLES OR VEX_BUILD_TESTS)
    set(VEX_ENABLE_SLANG_DEFAULT ON)
endif()

option(VEX_ENABLE_SHADER_COMPILER "Enables the possibility to use Vex's built-in ShaderCompiler." ON)
option(VEX_ENABLE_DXC "Enables the possibility to use the DXC shader compiler backend." ON)
option(VEX_ENABLE_SLANG "Enables the possibility to use the Slang shader compiler backend." ${VEX_ENABLE_SLANG_DEFAULT})

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
# Dependencies are declared with FetchContent, which looks for an installed package first when FIND_PACKAGE_ARGS is
# given (see the "Dependencies" section of the README). The graphics backend and shader compiler dependencies are
# declared in their respective cmake/Vex*.cmake files.
include(FetchContent)

# magic_enum is always built from source (its C++20 module is compiled into Vex) and installed alongside Vex.
# OVERRIDE_FIND_PACKAGE makes any find_package(magic_enum) in the build use this copy.
set(MAGIC_ENUM_OPT_INSTALL ON)
set(MAGIC_ENUM_OPT_INSTALL_PACKAGE_XML OFF)
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG b233b96e49d371bad00300f59b5ba581100b8745 # Use hash to select the specific commit we require (C++20 module support).
    OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(magic_enum)
# =========================================

# Set static library output
add_library(Vex STATIC
    # Platform
    "src/Vex/Platform/Platform.h"
    "src/Vex/Platform/Debug.h"
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
    "src/Vex/Utility/Hash.cpp"
    "src/Vex/Utility/MaybeUninitialized.h"
    "src/Vex/Utility/NonNullPtr.h"
    "src/Vex/Utility/ByteUtils.h"
    "src/Vex/Utility/WString.h"
    "src/Vex/Utility/Visitor.h"
    "src/Vex/Utility/Algorithms.h"
    "src/Vex/Utility/SHA1.h"
    "src/Vex/Utility/SHA1.cpp"
    "src/Vex/Utility/Functional/__functional_base.h"
    "src/Vex/Utility/Functional/function.h"
    "src/Vex/Utility/Functional/function_ref.h"
    "src/Vex/Utility/Functional/move_only_function.h"
    "src/Vex/Utility/MoveOnlyFunction.h"
    "src/Vex/Utility/MagicEnum.h"
    # Vex Platform
    "src/Vex/Platform/PlatformWindow.h"
    # Vex Containers
    "src/Vex/Containers/FreeList.h"
    "src/Vex/Containers/InlineVector.h"
    "src/Vex/Containers/Span.h"
    "src/Vex/Containers/beman/inplace_vector.hpp"
    # Vex Built-In Shaders
    "src/Vex/Generated/MipGeneration.h"
    # Vex API
    "src/Vex.h"
    "src/VexMacros.h"
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
    "src/Vex/BuildAccelerationStructure.h"
    "src/Vex/ShaderView.h"
    "src/Vex/RayTracing.h"
    "src/Vex/PipelineState.cpp"
    "src/Vex/PipelineState.h"
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
if (VEX_ENABLE_SHADER_COMPILER)
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

    include(VexEmbedShaders)
endif()

if(VEX_INTERNAL_USE_MODULES)
    target_sources(Vex PUBLIC
            FILE_SET CXX_MODULES
            BASE_DIRS
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${magic_enum_SOURCE_DIR}/module"
            FILES
            "src/Vex.cppm"
            "${magic_enum_SOURCE_DIR}/module/magic_enum.cppm"
    )
endif()

set_target_properties(Vex PROPERTIES CXX_SCAN_FOR_MODULES ${VEX_INTERNAL_USE_MODULES})
# Also needed by projects using an installed Vex: its exported module requires a C++ standard.
target_compile_features(Vex PUBLIC cxx_std_23)
# Same name as the installed target, for projects which add Vex with FetchContent or add_subdirectory.
add_library(Vex::Vex ALIAS Vex)

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

target_link_libraries(Vex PUBLIC magic_enum::magic_enum)

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

target_compile_definitions(Vex PUBLIC VEX_MODULES=$<BOOL:${VEX_INTERNAL_USE_MODULES}>)

# Shader compiler defines
target_compile_definitions(Vex PUBLIC VEX_SHADER_COMPILER=$<BOOL:${VEX_ENABLE_SHADER_COMPILER}>)
target_compile_definitions(Vex PUBLIC VEX_DXC=$<BOOL:${VEX_ENABLE_DXC}>)
target_compile_definitions(Vex PUBLIC VEX_SLANG=$<BOOL:${VEX_ENABLE_SLANG}>)

# =========================================
# INSTALLATION
# =========================================
# Installation rules
if(VEX_INTERNAL_USE_MODULES)
    set(VEX_INSTALL_MODULES FILE_SET CXX_MODULES DESTINATION modules)
endif()
install(TARGETS Vex
        EXPORT VexTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        ${VEX_INSTALL_MODULES}
)

# Install Vex headers
install(DIRECTORY src/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

# Files used by vex_setup_runtime, VexConfig.cmake points VEX_ROOT_DIR at this directory.
install(DIRECTORY shaders DESTINATION share/Vex)
install(FILES src/DX12/DX12AgilitySDK.cpp DESTINATION share/Vex/src/DX12)

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
if(TARGET unofficial::spirv-reflect)
    set(VEX_SPIRV_REFLECT_FROM_PACKAGE ON)
else()
    set(VEX_SPIRV_REFLECT_FROM_PACKAGE OFF)
endif()
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

# The dependency scripts are run again by VexConfig.cmake, to find or download the same dependencies.
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/VexConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/VexConfigVersion.cmake"
    cmake/VexHelpers.cmake
    cmake/VexDX12.cmake
    cmake/VexDXC.cmake
    cmake/VexSlang.cmake
    DESTINATION lib/cmake/Vex
)
