cmake_minimum_required(VERSION 4.2.2)
project(Vex VERSION 0.0.5 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(VEX_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Vex")
set(VEX_INSTALL_SHADERDIR "${CMAKE_INSTALL_DATADIR}/Vex/shaders")

# =========================================
# OPTIONS
# =========================================
# Examples, tests and tools are used to develop Vex, they should not be used or included in projects that use Vex (to enable them modify your CMakePresets.json).
option(VEX_BUILD_EXAMPLES "Build the example programs" OFF)
option(VEX_BUILD_TESTS "Build Vex tests" OFF)
option(VEX_BUILD_TOOLS "Build Vex tools" OFF)

set(VEX_GRAPHICS_BACKEND "AUTO" CACHE STRING "Graphics backend to use (AUTO, DX12, VULKAN)")
set_property(CACHE VEX_GRAPHICS_BACKEND PROPERTY STRINGS "AUTO" "DX12" "VULKAN")

set(VEX_ENABLE_SLANG_DEFAULT OFF)
if(VEX_BUILD_EXAMPLES OR VEX_BUILD_TESTS)
    set(VEX_ENABLE_SLANG_DEFAULT ON)
endif()
option(VEX_ENABLE_SHADER_COMPILER "Builds Vex's ShaderCompiler library (Vex::ShaderCompiler)." ON)
option(VEX_ENABLE_DXC "Enables the possibility to use the DXC shader compiler backend." ON)
option(VEX_ENABLE_SLANG "Enables the possibility to use the Slang shader compiler backend." ${VEX_ENABLE_SLANG_DEFAULT})

option(VEX_FIND_DEPENDENCIES "Use installed copies of Vex's dependencies (find_package) when available, instead of always downloading the pinned ones." ON)
set(VEX_AGILITY_SDK_ROOT "" CACHE PATH "Extracted Microsoft.Direct3D.D3D12 NuGet package to use, downloaded when empty.")

# Side-note regarding CMake modules support, if you wish to use "import std;" in your project, you must set the CMAKE_EXPERIMENTAL_CXX_IMPORT_STD variable to the value indicated here:
# https://github.com/Kitware/CMake/blob/v4.4.0-rc2/Help/dev/experimental.rst
# This feature flag depends on your CMake version, so make sure to select the appropriate branch.
option(VEX_USE_MODULES "Determines whether to generate and export the Vex cpp20 modules (experimental due to flaky compiler/build-system support)." OFF)

# Detect clang-cl (Clang frontend with MSVC ABI has no module scanning support)
# This is fixed in CMake 4.4.0.
# See https://gitlab.kitware.com/cmake/cmake/-/merge_requests/11875
set(VEX_INTERNAL_USE_MODULES ${VEX_USE_MODULES})
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC" AND CMAKE_VERSION VERSION_LESS "4.4")
    set(VEX_INTERNAL_USE_MODULES OFF)
    message(STATUS "Detected clang-cl: C++20 module scanning not supported, disabling modules. This will be fixed in CMake 4.4.0.")
endif()

# Resolve the graphics backend, AUTO selects DX12 on Windows and Vulkan elsewhere.
set(VEX_SELECTED_BACKEND "${VEX_GRAPHICS_BACKEND}")
if(VEX_SELECTED_BACKEND STREQUAL "AUTO")
    if(WIN32)
        set(VEX_SELECTED_BACKEND "DX12")
    else()
        set(VEX_SELECTED_BACKEND "VULKAN")
    endif()
endif()
if(NOT VEX_SELECTED_BACKEND MATCHES "^(DX12|VULKAN)$")
    message(FATAL_ERROR "Invalid VEX_GRAPHICS_BACKEND: ${VEX_GRAPHICS_BACKEND}. Must be AUTO, DX12, or VULKAN")
elseif(VEX_SELECTED_BACKEND STREQUAL "DX12" AND NOT WIN32)
    message(FATAL_ERROR "DirectX 12 backend is only available on Windows")
endif()
message(STATUS "Selected graphics backend: ${VEX_SELECTED_BACKEND}")

# Kept in the cache so that projects embedding Vex can query the selected backend.
string(COMPARE EQUAL "${VEX_SELECTED_BACKEND}" "DX12" VEX_ENABLE_DX12)
string(COMPARE EQUAL "${VEX_SELECTED_BACKEND}" "VULKAN" VEX_ENABLE_VULKAN)
set(VEX_ENABLE_DX12 ${VEX_ENABLE_DX12} CACHE INTERNAL "DirectX 12 backend enabled")
set(VEX_ENABLE_VULKAN ${VEX_ENABLE_VULKAN} CACHE INTERNAL "Vulkan backend enabled")

# =========================================
# DEPENDENCIES
# =========================================
include(FetchContent)

# magic_enum is always fetched and shipped with Vex.
message(STATUS "Fetching magic_enum...")
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG b233b96e49d371bad00300f59b5ba581100b8745 # Use hash to select the specific commit we require (C++20 module support).
)
FetchContent_MakeAvailable(magic_enum)

# Every other dependency is looked up first, and only downloaded when missing (creates the VexDeps:: targets).
include(VexDependencies)
vex_resolve_dependencies()

# =========================================
# VEX
# =========================================
file(GLOB_RECURSE VEX_SOURCES CONFIGURE_DEPENDS "src/Vex/*.cpp" "src/RHI/*.cpp")
file(GLOB_RECURSE VEX_HEADERS CONFIGURE_DEPENDS "src/Vex/*.h" "src/Vex/*.hpp" "src/RHI/*.h")
if(VEX_ENABLE_DX12)
    file(GLOB_RECURSE VEX_BACKEND_SOURCES CONFIGURE_DEPENDS "src/DX12/*.cpp")
    file(GLOB_RECURSE VEX_BACKEND_HEADERS CONFIGURE_DEPENDS "src/DX12/*.h")
    # Compiled by executables using Vex instead (see vex_setup_runtime).
    list(FILTER VEX_BACKEND_SOURCES EXCLUDE REGEX "DX12AgilitySDK\\.cpp$")
else()
    file(GLOB_RECURSE VEX_BACKEND_SOURCES CONFIGURE_DEPENDS "src/Vulkan/*.cpp")
    file(GLOB_RECURSE VEX_BACKEND_HEADERS CONFIGURE_DEPENDS "src/Vulkan/*.h")
endif()

add_library(Vex STATIC ${VEX_SOURCES} ${VEX_BACKEND_SOURCES})
add_library(Vex::Vex ALIAS Vex)
target_sources(Vex PUBLIC FILE_SET HEADERS BASE_DIRS "src"
    FILES "src/Vex.h" "src/VexMacros.h" ${VEX_HEADERS} ${VEX_BACKEND_HEADERS})
if(VEX_INTERNAL_USE_MODULES)
    target_sources(Vex PUBLIC FILE_SET CXX_MODULES
        BASE_DIRS "src" "${magic_enum_SOURCE_DIR}/module"
        FILES "src/Vex.cppm" "${magic_enum_SOURCE_DIR}/module/magic_enum.cppm")
endif()
set_target_properties(Vex PROPERTIES CXX_SCAN_FOR_MODULES ${VEX_INTERNAL_USE_MODULES})
target_compile_features(Vex PUBLIC cxx_std_23)

# magic_enum's headers are installed with Vex's own.
target_link_libraries(Vex PUBLIC $<BUILD_INTERFACE:magic_enum::magic_enum>)
if(VEX_ENABLE_DX12)
    target_link_libraries(Vex PUBLIC VexDeps::agility d3d12 dxgi dxguid PRIVATE VexDeps::pix)
    get_target_property(VEX_AGILITY_SDK_VERSION VexDeps::agility VEX_SDK_VERSION)
    target_compile_definitions(Vex PRIVATE DIRECTX_AGILITY_SDK_VERSION=${VEX_AGILITY_SDK_VERSION} D3D12_AGILITY_SDK_ENABLED)
else()
    target_link_libraries(Vex PUBLIC Vulkan::Vulkan)
endif()

# Debug: Full debug symbols
# Development: Optimizations with some debug symbols
# Shipping: Fully optimized build without debug symbols
target_compile_definitions(Vex PUBLIC
    $<$<CONFIG:Debug>:VEX_DEBUG=1 VEX_DEVELOPMENT=0 VEX_SHIPPING=0>
    $<$<CONFIG:RelWithDebInfo>:VEX_DEBUG=0 VEX_DEVELOPMENT=1 VEX_SHIPPING=0>
    $<$<CONFIG:Release>:VEX_DEBUG=0 VEX_DEVELOPMENT=0 VEX_SHIPPING=1>
    VEX_DX12=$<BOOL:${VEX_ENABLE_DX12}>
    VEX_VULKAN=$<BOOL:${VEX_ENABLE_VULKAN}>
    VEX_MODULES=$<BOOL:${VEX_INTERNAL_USE_MODULES}>
    # Avoid windows polluting the code with macros and allow for typically "unsafe" C functions.
    $<$<PLATFORM_ID:Windows>:NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS>
)
target_compile_definitions(Vex PRIVATE $<$<BOOL:${VEX_BUILD_TESTS}>:VEX_TESTS=1>)

# =========================================
# VEX SHADER COMPILER
# =========================================
if(VEX_ENABLE_SHADER_COMPILER)
    file(GLOB VEX_SHADER_COMPILER_SOURCES CONFIGURE_DEPENDS "src/ShaderCompiler/*.cpp")
    file(GLOB_RECURSE VEX_SHADER_COMPILER_HEADERS CONFIGURE_DEPENDS "src/ShaderCompiler/*.h")

    add_library(VexShaderCompiler STATIC ${VEX_SHADER_COMPILER_SOURCES}
        "src/ShaderCompiler/Compiler/DXC/DXCReflection.cpp")
    add_library(Vex::ShaderCompiler ALIAS VexShaderCompiler)
    target_sources(VexShaderCompiler PUBLIC FILE_SET HEADERS BASE_DIRS "src"
        FILES "src/VexShaderCompiler.h" ${VEX_SHADER_COMPILER_HEADERS})
    if(VEX_INTERNAL_USE_MODULES)
        target_sources(VexShaderCompiler PUBLIC FILE_SET CXX_MODULES BASE_DIRS "src" FILES "src/VexShaderCompiler.cppm")
    endif()
    set_target_properties(VexShaderCompiler PROPERTIES
        EXPORT_NAME ShaderCompiler
        CXX_SCAN_FOR_MODULES ${VEX_INTERNAL_USE_MODULES}
    )

    target_link_libraries(VexShaderCompiler PUBLIC Vex)
    target_compile_definitions(VexShaderCompiler PUBLIC
        VEX_SHADER_COMPILER=1
        VEX_DXC=$<BOOL:${VEX_ENABLE_DXC}>
        VEX_SLANG=$<BOOL:${VEX_ENABLE_SLANG}>
    )
    if(VEX_ENABLE_DXC)
        target_sources(VexShaderCompiler PRIVATE "src/ShaderCompiler/Compiler/DXC/DXCCompiler.cpp")
        target_link_libraries(VexShaderCompiler PUBLIC VexDeps::dxc)
    endif()
    if(VEX_ENABLE_SLANG)
        target_sources(VexShaderCompiler PRIVATE "src/ShaderCompiler/Compiler/Slang/SlangCompiler.cpp")
        target_link_libraries(VexShaderCompiler PUBLIC VexDeps::slang)
    endif()
    if(VEX_SPIRV_REFLECT_DIR)
        target_sources(VexShaderCompiler PRIVATE "${VEX_SPIRV_REFLECT_DIR}/spirv_reflect.c")
        target_include_directories(VexShaderCompiler PRIVATE "${VEX_SPIRV_REFLECT_DIR}")
    elseif(VEX_ENABLE_VULKAN)
        target_link_libraries(VexShaderCompiler PRIVATE VexDeps::spirv_reflect)
    endif()

    include(VexEmbedShaders)
    list(APPEND VEX_TARGETS VexShaderCompiler)
endif()

# =========================================
# RUNTIME SETUP (vex_setup_runtime)
# =========================================
set_property(GLOBAL PROPERTY VEX_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set_property(GLOBAL PROPERTY VEX_AGILITY_SDK_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/DX12/DX12AgilitySDK.cpp")
include(VexRuntime)

# =========================================
# INSTALLATION
# =========================================
if(VEX_INTERNAL_USE_MODULES)
    set(VEX_INSTALL_MODULES FILE_SET CXX_MODULES DESTINATION "${CMAKE_INSTALL_DATADIR}/Vex/modules")
endif()
install(TARGETS Vex ${VEX_TARGETS}
    EXPORT VexTargets
    FILE_SET HEADERS
    ${VEX_INSTALL_MODULES}
)
install(EXPORT VexTargets NAMESPACE Vex:: DESTINATION "${VEX_INSTALL_CMAKEDIR}")

install(DIRECTORY "${magic_enum_SOURCE_DIR}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY "shaders/" DESTINATION "${VEX_INSTALL_SHADERDIR}")
if(VEX_ENABLE_DX12)
    install(FILES "src/DX12/DX12AgilitySDK.cpp" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/DX12")
endif()

# Tells the package config which dependencies were installed alongside Vex, and which ones have to be found again.
set(VEX_DEPS_BUNDLED "")
foreach(DEP agility pix dxc slang spirv_reflect)
    get_property(DEP_IS_RESOLVED GLOBAL PROPERTY VEX_DEP_${DEP}_BUNDLED SET)
    if(DEP_IS_RESOLVED)
        get_property(DEP_IS_BUNDLED GLOBAL PROPERTY VEX_DEP_${DEP}_BUNDLED)
        string(APPEND VEX_DEPS_BUNDLED "set(VEX_DEP_${DEP}_BUNDLED ${DEP_IS_BUNDLED})\n")
    endif()
endforeach()

# Config files for find_package()
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/VexConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VexConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/VexConfig.cmake"
    INSTALL_DESTINATION "${VEX_INSTALL_CMAKEDIR}"
    PATH_VARS VEX_INSTALL_SHADERDIR CMAKE_INSTALL_INCLUDEDIR
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/VexConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/VexConfigVersion.cmake"
    "cmake/VexDependencies.cmake"
    "cmake/VexRuntime.cmake"
    DESTINATION "${VEX_INSTALL_CMAKEDIR}"
)

# =========================================
# DEVELOPMENT (examples, tests, tools)
# =========================================
if(PROJECT_IS_TOP_LEVEL)
    # Will correctly install our custom githook to run clang-format on staged files when committing
    include(GitHooks)
endif()
if(VEX_BUILD_EXAMPLES OR VEX_BUILD_TESTS)
    include(VexDevDependencies)
endif()
if(VEX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(VEX_BUILD_TESTS)
    add_subdirectory(tests)
endif()
if(VEX_BUILD_TOOLS)
    add_subdirectory(tools/GenerateBuiltInShaders)
endif()
