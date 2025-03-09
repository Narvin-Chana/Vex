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
# =========================================

# Set static library output
add_library(Vex STATIC 
    src/Vex.cpp
    src/Vex.h
 "src/GfxBackend.h" "src/GfxBackend.cpp" "src/Formats.h" "src/PlatformWindow.h" "src/Types.h")

target_include_directories(Vex PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# DX12 project files
set(VEX_DX12_SOURCES
)

# Vulkan project files
set(VEX_VULKAN_SOURCES
)

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
    target_sources(Vex PRIVATE ${VEX_DX12_SOURCES})
    target_link_libraries(Vex PUBLIC d3d12.lib dxgi.lib)
    target_compile_definitions(Vex PUBLIC VEX_DX12)
endif()

# Static linking to Vulkan
if(VEX_ENABLE_VULKAN AND Vulkan_FOUND)
    target_sources(Vex PRIVATE ${VEX_VULKAN_SOURCES})
    target_link_libraries(Vex PUBLIC Vulkan::Vulkan)
    # Make Vulkan-Hpp headers available
    add_header_only_dependency(Vex vulkanhpp "${vulkanhpp_SOURCE_DIR}" "include" "vulkan")
    target_compile_definitions(Vex PUBLIC VEX_VULKAN)
endif()

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

# Clang Format and Clang Tidy Setup
find_program(VCPKG_EXE vcpkg)
if(NOT VCPKG_EXE)
    message(STATUS "vcpkg not found. Consider installing it to manage dependencies.")
    message(STATUS "Visit: https://github.com/microsoft/vcpkg")
    message(STATUS "Then install clang tools with: vcpkg install clang:x64-windows")
endif()

message(STATUS "Searching for clang-format...")
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

message(STATUS "Searching for clang-tidy...")
find_program(CLANG_TIDY_EXE clang-tidy)
if(CLANG_TIDY_EXE)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")

    # Set up clang-tidy command
    set(CMAKE_CXX_CLANG_TIDY 
        ${CLANG_TIDY_EXE};
    )

    # Right now I've disabled clang-tidy, because it destroys performance, we could activate it as a CI action on github or pre-commit git hook if we really want it.
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
else()
    message(STATUS "clang-tidy not found, static analysis integration will not be available")
endif()
