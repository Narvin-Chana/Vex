cmake_minimum_required(VERSION 4.2.2)
project(VexInstalledDepsTest LANGUAGES CXX C)

# Checks that Vex can be built purely from installed dependency packages, without FetchContent downloading anything:
#  - every dependency with a CMake package is made REQUIRED (CMAKE_REQUIRE_FIND_PACKAGE_<name>, with the FetchContent
#    name which is also the name find_package is called with), so a missing or too old package fails the configure
#    step instead of falling back to a download,
#  - magic_enum and the D3D12 Agility SDK, which Vex always builds itself, come from local copies
#    (FETCHCONTENT_SOURCE_DIR_<NAME>),
#  - configuring fails if FetchContent still populated any dependency.
#
# The packages are taken from VEX_TEST_PACKAGES_DIR. When empty, packages/ (standing in for a package manager) is
# installed into the build folder on the first configure.

if(NOT WIN32 AND NOT LINUX)
    message(FATAL_ERROR "This test only stages the Windows and Linux packages.")
endif()

set(VEX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Vex checkout to embed with add_subdirectory.")
set(VEX_TEST_PACKAGES_DIR "" CACHE PATH "Prefix with Vex's dependencies installed as packages (staged when empty).")

# Same backend selection as Vex (AUTO: DX12 on Windows, Vulkan elsewhere), to know which dependencies are needed.
set(VEX_GRAPHICS_BACKEND "AUTO" CACHE STRING "Graphics backend to use (AUTO, DX12, VULKAN)")
set(backend "${VEX_GRAPHICS_BACKEND}")
if(backend STREQUAL "AUTO")
    if(WIN32)
        set(backend "DX12")
    else()
        set(backend "VULKAN")
    endif()
endif()

# =========================================
# INSTALLED PACKAGES
# =========================================
set(packages_dir "${VEX_TEST_PACKAGES_DIR}")
if(NOT packages_dir)
    set(packages_dir "${CMAKE_CURRENT_BINARY_DIR}/packages")
    if(NOT EXISTS "${packages_dir}/lib/cmake/directx-dxc")
        message(STATUS "Staging the dependency packages into ${packages_dir}...")
        set(staging_dir "${CMAKE_CURRENT_BINARY_DIR}/packages-build")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_SOURCE_DIR}/packages" -B "${staging_dir}"
                -G "${CMAKE_GENERATOR}" -DCMAKE_BUILD_TYPE=Release -DVEX_GRAPHICS_BACKEND=${backend}
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            COMMAND_ERROR_IS_FATAL ANY
        )
        execute_process(COMMAND "${CMAKE_COMMAND}" --build "${staging_dir}" --config Release COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND "${CMAKE_COMMAND}" --install "${staging_dir}" --config Release --prefix "${packages_dir}"
            COMMAND_ERROR_IS_FATAL ANY)
    endif()
endif()
list(APPEND CMAKE_PREFIX_PATH "${packages_dir}")

# FetchContent names of the dependencies found as packages (directx-dxc, slang, winpixevent and
# unofficial-spirv-reflect packages).
set(required_packages dxc slang)
if(backend STREQUAL "DX12")
    list(APPEND required_packages winpixevent)
    set(FETCHCONTENT_SOURCE_DIR_D3D12_AGILITY_SDK "${packages_dir}/src/d3d12_agility_sdk")
else()
    list(APPEND required_packages spirv_reflect)
endif()
foreach(package IN LISTS required_packages)
    set(CMAKE_REQUIRE_FIND_PACKAGE_${package} ON)
endforeach()
set(FETCHCONTENT_SOURCE_DIR_MAGIC_ENUM "${packages_dir}/src/magic_enum")

# =========================================
# VEX
# =========================================
set(VEX_ENABLE_SLANG ON)
add_subdirectory("${VEX_SOURCE_DIR}" vex)

# None of the dependencies may have been downloaded.
set(fetched_dependencies ${required_packages} magic_enum)
if(backend STREQUAL "DX12")
    list(APPEND fetched_dependencies d3d12_agility_sdk)
endif()
foreach(dependency IN LISTS fetched_dependencies)
    FetchContent_GetProperties(${dependency})
    set(installed OFF)
    if(${dependency}_SOURCE_DIR)
        cmake_path(IS_PREFIX packages_dir "${${dependency}_SOURCE_DIR}" NORMALIZE installed)
    endif()
    if(${dependency}_SOURCE_DIR AND NOT installed)
        message(FATAL_ERROR "FetchContent downloaded ${dependency} to ${${dependency}_SOURCE_DIR} instead of Vex "
            "using the installed copy.")
    endif()
endforeach()
foreach(package IN LISTS required_packages)
    message(STATUS "VexInstalledDepsTest: ${package} from ${${package}_DIR}")
endforeach()

# =========================================
# TEST EXECUTABLE
# =========================================
add_executable(vex_installed_deps_test "main.cpp")
target_link_libraries(vex_installed_deps_test PRIVATE Vex::Vex)
target_compile_definitions(vex_installed_deps_test PRIVATE SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders")
vex_setup_runtime(vex_installed_deps_test)
