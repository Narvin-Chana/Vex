# DirectX Shader Compiler, provides Microsoft::DirectXShaderCompiler (the target name of vcpkg's directx-dxc port).
#
# An installed directx-dxc package (at least DXC_VERSION) is used when found, otherwise the pinned release is
# downloaded. See the "Dependencies" section of the README for the CMake variables controlling this.
include(FetchContent)

if(NOT VEX_ENABLE_SHADER_COMPILER OR NOT VEX_ENABLE_DXC)
    function(build_with_dxc target)
    endfunction()
    return()
endif()

set(DXC_VERSION 1.9.2602)
set(DXC_RELEASE_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v${DXC_VERSION}")
if(WIN32)
    set(DXC_URL "${DXC_RELEASE_URL}/dxc_2026_02_20.zip")
    set(DXC_SHA256 a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e)
elseif(LINUX)
    set(DXC_URL "${DXC_RELEASE_URL}/linux_dxc_2026_02_20.x86_64.tar.gz")
    set(DXC_SHA256 a1d3e3b5e1c5685b3eb27d5e8890e41d87df45def05112a2d6f1a63a931f7d60)
else()
    message(FATAL_ERROR "Unsupported platform for DXC binaries")
endif()

FetchContent_Declare(dxc
    URL "${DXC_URL}"
    URL_HASH SHA256=${DXC_SHA256}
    FIND_PACKAGE_ARGS ${DXC_VERSION} NAMES directx-dxc CONFIG GLOBAL
)
FetchContent_MakeAvailable(dxc)

# Not found as a package: the release archive was downloaded instead.
if(NOT TARGET Microsoft::DirectXShaderCompiler)
    add_library(Microsoft::DirectXShaderCompiler SHARED IMPORTED GLOBAL)
    if(WIN32)
        set_target_properties(Microsoft::DirectXShaderCompiler PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${dxc_SOURCE_DIR}/inc"
            IMPORTED_LOCATION "${dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll"
            IMPORTED_IMPLIB "${dxc_SOURCE_DIR}/lib/x64/dxcompiler.lib"
        )
    else()
        set_target_properties(Microsoft::DirectXShaderCompiler PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${dxc_SOURCE_DIR}/include"
            IMPORTED_LOCATION "${dxc_SOURCE_DIR}/lib/libdxcompiler.so"
        )
    endif()
endif()

function(build_with_dxc target)
    # Public: DXC's headers are part of Vex's ShaderCompiler headers.
    target_link_libraries(${target} PUBLIC Microsoft::DirectXShaderCompiler)
    target_sources(${target} PRIVATE
        "src/ShaderCompiler/Compiler/DXC/DXCCompiler.h"
        "src/ShaderCompiler/Compiler/DXC/DXCCompiler.cpp"
    )
endfunction()
