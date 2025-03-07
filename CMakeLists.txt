cmake_minimum_required(VERSION 3.27)
project(Vex VERSION 0.0.1 LANGUAGES CXX)
include(CMakePackageConfigHelpers)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# Clang Format and Clang Tidy Setup
find_program(VCPKG_EXE vcpkg)
if(NOT VCPKG_EXE)
    message(STATUS "vcpkg not found. Consider installing it to manage dependencies.")
    message(STATUS "Visit: https://github.com/microsoft/vcpkg")
    message(STATUS "Then install clang tools with: vcpkg install clang:x64-windows")
endif()

find_program(CLANG_FORMAT_EXE clang-format)
if(CLANG_FORMAT_EXE)
    message(STATUS "Found clang-format: ${CLANG_FORMAT_EXE}")
    
    # Get all project source files
    file(GLOB_RECURSE ALL_SOURCE_FILES 
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.h
        ${CMAKE_SOURCE_DIR}/include/*.h
    )
    
    # Remove any files you don't want to format
    # list(FILTER ALL_SOURCE_FILES EXCLUDE REGEX "some/path/to/exclude")
    
    # Format target
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting source code with clang-format"
        VERBATIM
    )
else()
    message(STATUS "clang-format not found, format target will not be available")
endif()

find_program(CLANG_TIDY_EXE clang-tidy)
if(CLANG_TIDY_EXE)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")

    # Set up clang-tidy command
    set(CMAKE_CXX_CLANG_TIDY 
        ${CLANG_TIDY_EXE};
    )

    # Right now I've disabled clang-tidy, because it destroys performance, we could activate it as a CI action on github if we really want it.
    #set_target_properties(Vex PROPERTIES
    #   CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}"
    #)

    # Allow excluding certain targets from clang-tidy
    function(exclude_from_clang_tidy target)
        set_target_properties(${target} PROPERTIES
            C_CLANG_TIDY ""
            CXX_CLANG_TIDY ""
        )
    endfunction()
    
    # Exclude external libraries (otherwise clang-tidy will take ages to run)
    exclude_from_clang_tidy(efsw)
    exclude_from_clang_tidy(efsw-static)
    exclude_from_clang_tidy(vex_efsw_headers)
else()
    message(STATUS "clang-tidy not found, static analysis integration will not be available")
endif()
