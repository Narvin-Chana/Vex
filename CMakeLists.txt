cmake_minimum_required(VERSION 3.27)
project(Vex VERSION 0.0.1 LANGUAGES CXX)
include(CMakePackageConfigHelpers)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    option(VEX_BUILD_EXAMPLES "Build the example programs" ON)
else()
    option(VEX_BUILD_EXAMPLES "Build the example programs" OFF)
endif()

# D3D12 is only available on Windows
if (WIN32)
    option(VEX_ENABLE_DX12 "Enable DirectX 12 backend" ON)
endif()
# Vulkan is available on Windows and Linux
option(VEX_ENABLE_VULKAN "Enable Vulkan backend" ON)

# Fetch efsw dependency
include(FetchContent)
FetchContent_Declare(
    efsw
    GIT_REPOSITORY https://github.com/SpartanJ/efsw.git
    GIT_TAG        1.4.1
)
FetchContent_MakeAvailable(efsw)

# Set static library output
add_library(Vex STATIC 
    src/Vex.cpp
    src/Vex.h
)

target_include_directories(Vex PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# Link efsw
target_link_libraries(Vex PUBLIC efsw)

# Create a custom target to handle esfw headers
add_library(vex_efsw_headers INTERFACE)
target_include_directories(vex_efsw_headers INTERFACE
    $<BUILD_INTERFACE:${efsw_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/efsw>
)
target_link_libraries(Vex PUBLIC vex_efsw_headers)

# Installation rules
install(TARGETS Vex vex_efsw_headers efsw
    EXPORT VexTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install Vex headers
install(DIRECTORY src/ DESTINATION include FILES_MATCHING PATTERN "*.h")

# Install efsw headers
install(DIRECTORY ${efsw_SOURCE_DIR}/include/ DESTINATION include)

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
