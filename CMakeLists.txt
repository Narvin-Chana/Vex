cmake_minimum_required(VERSION 3.27)
project(Vex VERSION 0.0.1 LANGUAGES CXX)
include(CMakePackageConfigHelpers)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Will correctly install our custom githook to run clang-format on staged files when committing
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitHooks.cmake)

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
    message(WARNING "No graphics backends available. Some functionality may be limited.")
endif()

# =========================================
# DEPENDENCIES FETCHING
# =========================================
# Fetch efsw dependency
message(STATUS "Fetching efsw...")
include(FetchContent)
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

#Fetch magic_enum dependency
message(STATUS "Fetching magic_enum...")
include(FetchContent)
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
    "src/Vex/FeatureChecker.cpp" 
    "src/Vex/RHI.h"  
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
        "src/DX12/DX12Formats.h" 
        "src/DX12/DX12FeatureChecker.h"
        "src/DX12/DX12FeatureChecker.cpp"
        "src/DX12/DX12RHI.h"
        "src/DX12/DX12RHI.cpp"
        "src/DX12/DX12Headers.h"
        "src/DX12/HRChecker.h"
        "src/DX12/HRChecker.cpp"
        "src/DX12/DXGIFactory.h"
        "src/DX12/DXGIFactory.cpp"
    )
endif()

# Vulkan project files
if (VEX_ENABLE_VULKAN AND Vulkan_FOUND)
    set(VEX_VULKAN_SOURCES
        "src/Vulkan/VkFormats.h" 
        "src/Vulkan/VkFeatureChecker.h"
        "src/Vulkan/VkFeatureChecker.cpp"
        "src/Vulkan/VkRHI.h"
        "src/Vulkan/VkRHI.cpp"
        "src/Vulkan/VkDevice.cpp"
        "src/Vulkan/VkExtensions.cpp"
        "src/Vulkan/VkDebug.h"
        "src/Vulkan/VkDebug.cpp"
        "src/Vulkan/VkErrorHandler.h"
        "src/Vulkan/VkErrorHandler.cpp"
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
    target_sources(Vex PRIVATE ${VEX_DX12_SOURCES})
    target_link_libraries(Vex PUBLIC d3d12.lib dxgi.lib)
    target_compile_definitions(Vex PUBLIC VEX_DX12=1)
else()
    target_compile_definitions(Vex PUBLIC VEX_DX12=0)
endif()

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
