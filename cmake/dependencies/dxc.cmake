# DirectX Shader Compiler (vcpkg: directx-dxc).
function(_vex_resolve_dxc)
    if(TARGET VexDeps::dxc)
        return()
    endif()
    set(version 1.9.2602)
    set(release "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v${version}")
    if(WIN32)
        _vex_find_dependency(dxc PACKAGE directx-dxc VERSION ${version} HEADER dxcapi.h
            TARGETS Microsoft::DirectXShaderCompiler)
        if(NOT TARGET VexDeps::dxc)
            _vex_download(dxc root "${release}/dxc_2026_02_20.zip"
                a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e)
            _vex_imported_library(dxc "${root}/inc"
                "${root}/bin/x64/dxcompiler.dll" "${root}/lib/x64/dxcompiler.lib")
            _vex_bundle(dxc "${root}" inc bin/x64/dxcompiler.dll lib/x64/dxcompiler.lib)
        endif()
    elseif(LINUX)
        _vex_find_dependency(dxc PACKAGE directx-dxc VERSION ${version} HEADER dxc/dxcapi.h
            TARGETS Microsoft::DirectXShaderCompiler)
        if(NOT TARGET VexDeps::dxc)
            _vex_download(dxc root "${release}/linux_dxc_2026_02_20.x86_64.tar.gz"
                a1d3e3b5e1c5685b3eb27d5e8890e41d87df45def05112a2d6f1a63a931f7d60)
            _vex_imported_library(dxc "${root}/include" "${root}/lib/libdxcompiler.so")
            _vex_bundle(dxc "${root}" include lib/libdxcompiler.so)
        endif()
    else()
        message(FATAL_ERROR "Unsupported platform for DXC binaries")
    endif()
endfunction()
