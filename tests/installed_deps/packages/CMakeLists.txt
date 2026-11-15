cmake_minimum_required(VERSION 4.2.2)
project(VexTestPackages LANGUAGES C CXX)

# Stands in for a package manager: installs Vex's dependencies, at the versions Vex pins, as CMake packages (with
# version files, and the target names vcpkg's ports use) into CMAKE_INSTALL_PREFIX. Also installs the sources of the
# dependencies Vex always builds itself (magic_enum, the D3D12 Agility SDK) under src/.
#
# Keep the versions in sync with cmake/VexDXC.cmake, VexSlang.cmake, VexDX12.cmake, VexVulkan.cmake and CMakeLists.txt.

if(NOT WIN32 AND NOT LINUX)
    message(FATAL_ERROR "Only the Windows and Linux packages are staged.")
endif()
if(NOT VEX_GRAPHICS_BACKEND MATCHES "^(DX12|VULKAN)$")
    message(FATAL_ERROR "VEX_GRAPHICS_BACKEND must be DX12 or VULKAN.")
endif()

include(FetchContent)
include(CMakePackageConfigHelpers)

# Writes and installs <package>-config.cmake (with the given content) and <package>-config-version.cmake.
function(install_package package version content)
    set(dir "${CMAKE_CURRENT_BINARY_DIR}/${package}")
    file(WRITE "${dir}/${package}-config.cmake"
        "get_filename_component(_prefix \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n${content}")
    write_basic_package_version_file("${dir}/${package}-config-version.cmake"
        VERSION ${version} COMPATIBILITY AnyNewerVersion)
    install(FILES "${dir}/${package}-config.cmake" "${dir}/${package}-config-version.cmake"
        DESTINATION "lib/cmake/${package}")
endfunction()

# =========================================
# DXC (directx-dxc, Microsoft::DirectXShaderCompiler)
# =========================================
set(dxc_release "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602")
if(WIN32)
    FetchContent_Declare(pkg_dxc
        URL "${dxc_release}/dxc_2026_02_20.zip"
        URL_HASH SHA256=a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e
    )
    FetchContent_MakeAvailable(pkg_dxc)
    install(DIRECTORY "${pkg_dxc_SOURCE_DIR}/inc/" DESTINATION include)
    install(FILES "${pkg_dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll" "${pkg_dxc_SOURCE_DIR}/bin/x64/dxil.dll" DESTINATION bin)
    install(FILES "${pkg_dxc_SOURCE_DIR}/lib/x64/dxcompiler.lib" DESTINATION lib)
    set(dxc_properties "IMPORTED_LOCATION \"\${_prefix}/bin/dxcompiler.dll\" IMPORTED_IMPLIB \"\${_prefix}/lib/dxcompiler.lib\"")
else()
    FetchContent_Declare(pkg_dxc
        URL "${dxc_release}/linux_dxc_2026_02_20.x86_64.tar.gz"
        URL_HASH SHA256=a1d3e3b5e1c5685b3eb27d5e8890e41d87df45def05112a2d6f1a63a931f7d60
    )
    FetchContent_MakeAvailable(pkg_dxc)
    install(DIRECTORY "${pkg_dxc_SOURCE_DIR}/include/" DESTINATION include)
    install(FILES "${pkg_dxc_SOURCE_DIR}/lib/libdxcompiler.so" "${pkg_dxc_SOURCE_DIR}/lib/libdxil.so" DESTINATION lib)
    set(dxc_properties "IMPORTED_LOCATION \"\${_prefix}/lib/libdxcompiler.so\"")
endif()
install_package(directx-dxc 1.9.2602 "
if(NOT TARGET Microsoft::DirectXShaderCompiler)
    add_library(Microsoft::DirectXShaderCompiler SHARED IMPORTED)
    set_target_properties(Microsoft::DirectXShaderCompiler PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES \"\${_prefix}/include\" ${dxc_properties})
endif()
")

# =========================================
# Slang (slang, slang::slang): its release is already a CMake package, installed as is.
# =========================================
set(slang_release "https://github.com/shader-slang/slang/releases/download/v2026.18/slang-2026.18")
if(WIN32)
    FetchContent_Declare(pkg_slang
        URL "${slang_release}-windows-x86_64.zip"
        URL_HASH SHA256=6ffa4827b519fd0a85b38407049d87ab0c1f045fe2289cb1e6831f965169f8a1
    )
else()
    FetchContent_Declare(pkg_slang
        URL "${slang_release}-linux-x86_64.tar.gz"
        URL_HASH SHA256=03acde8d19ad17f7b94312078ed945bed554a220feff084898a1bc94a9c998f5
    )
endif()
FetchContent_MakeAvailable(pkg_slang)
install(DIRECTORY "${pkg_slang_SOURCE_DIR}/" DESTINATION . USE_SOURCE_PERMISSIONS)

# =========================================
# magic_enum: Vex always builds it from source (FETCHCONTENT_SOURCE_DIR_MAGIC_ENUM).
# =========================================
FetchContent_Declare(pkg_magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG b233b96e49d371bad00300f59b5ba581100b8745
    SOURCE_SUBDIR "<invalid path>" # Only the sources are installed.
)
FetchContent_MakeAvailable(pkg_magic_enum)
install(DIRECTORY "${pkg_magic_enum_SOURCE_DIR}/" DESTINATION src/magic_enum PATTERN ".git" EXCLUDE)

if(VEX_GRAPHICS_BACKEND STREQUAL "DX12")
    # =========================================
    # WinPixEventRuntime (winpixevent, Microsoft::WinPixEventRuntime)
    # =========================================
    FetchContent_Declare(pkg_pix
        URL "https://www.nuget.org/api/v2/package/WinPixEventRuntime/1.0.240308001"
        URL_HASH SHA256=726acc93d6968e2146261a1e415521747d50ad69894c2b42b5d0d4c29fd66ec4
        DOWNLOAD_NAME WinPixEventRuntime.zip
    )
    FetchContent_MakeAvailable(pkg_pix)
    install(DIRECTORY "${pkg_pix_SOURCE_DIR}/Include/" DESTINATION include)
    install(FILES "${pkg_pix_SOURCE_DIR}/bin/x64/WinPixEventRuntime.dll" DESTINATION bin)
    install(FILES "${pkg_pix_SOURCE_DIR}/bin/x64/WinPixEventRuntime.lib" DESTINATION lib)
    install_package(winpixevent 1.0.240308001 "
if(NOT TARGET Microsoft::WinPixEventRuntime)
    add_library(Microsoft::WinPixEventRuntime SHARED IMPORTED)
    set_target_properties(Microsoft::WinPixEventRuntime PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES \"\${_prefix}/include\"
        IMPORTED_LOCATION \"\${_prefix}/bin/WinPixEventRuntime.dll\"
        IMPORTED_IMPLIB \"\${_prefix}/lib/WinPixEventRuntime.lib\")
endif()
")

    # =========================================
    # D3D12 Agility SDK: has no CMake package, Vex takes it from FETCHCONTENT_SOURCE_DIR_D3D12_AGILITY_SDK.
    # =========================================
    FetchContent_Declare(pkg_agility
        URL "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.618.4"
        URL_HASH SHA256=e2049bccbc506a49e3f495c07ee1f97b05a1b833cf8b3db1cf93da56baf2ec53
        DOWNLOAD_NAME Microsoft.Direct3D.D3D12.zip
    )
    FetchContent_MakeAvailable(pkg_agility)
    install(DIRECTORY "${pkg_agility_SOURCE_DIR}/" DESTINATION src/d3d12_agility_sdk)
else()
    # =========================================
    # SPIRV-Reflect (unofficial-spirv-reflect, unofficial::spirv-reflect): upstream is a single source file, built here
    # as a static library like vcpkg does.
    # =========================================
    FetchContent_Declare(pkg_spirv_reflect
        URL "https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-1.4.321.0.tar.gz"
        URL_HASH SHA256=254ed36128e7abe8fb12ef804319d0790e059093c116e9fe55d4003880515f85
        SOURCE_SUBDIR "<invalid path>" # Upstream's CMakeLists also builds its tools.
    )
    FetchContent_MakeAvailable(pkg_spirv_reflect)
    add_library(spirv-reflect STATIC "${pkg_spirv_reflect_SOURCE_DIR}/spirv_reflect.c")
    set_target_properties(spirv-reflect PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(spirv-reflect PUBLIC
        "$<BUILD_INTERFACE:${pkg_spirv_reflect_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:include>"
    )
    install(TARGETS spirv-reflect EXPORT spirv-reflect ARCHIVE DESTINATION lib)
    install(EXPORT spirv-reflect NAMESPACE unofficial:: FILE unofficial-spirv-reflect-targets.cmake
        DESTINATION lib/cmake/unofficial-spirv-reflect)
    # spirv_reflect.h includes "./include/spirv/unified1/spirv.h", relative to itself.
    install(FILES "${pkg_spirv_reflect_SOURCE_DIR}/spirv_reflect.h" DESTINATION include)
    install(DIRECTORY "${pkg_spirv_reflect_SOURCE_DIR}/include" DESTINATION include)
    install_package(unofficial-spirv-reflect 1.4.321.0
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/unofficial-spirv-reflect-targets.cmake\")\n")
endif()
