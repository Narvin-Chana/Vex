# WinPixEventRuntime (vcpkg: winpixevent).
function(_vex_resolve_pix)
    if(TARGET VexDeps::pix)
        return()
    endif()
    set(version 1.0.240308001)
    _vex_find_dependency(pix PACKAGE winpixevent VERSION ${version} HEADER WinPixEventRuntime/pix3.h
        TARGETS Microsoft::WinPixEventRuntime)
    if(TARGET VexDeps::pix)
        return()
    endif()
    _vex_download(pix root
        "https://www.nuget.org/api/v2/package/WinPixEventRuntime/${version}"
        726acc93d6968e2146261a1e415521747d50ad69894c2b42b5d0d4c29fd66ec4
        DOWNLOAD_NAME pix.zip
    )
    _vex_imported_library(pix "${root}/Include"
        "${root}/bin/x64/WinPixEventRuntime.dll" "${root}/bin/x64/WinPixEventRuntime.lib")
    _vex_bundle(pix "${root}" Include bin/x64/WinPixEventRuntime.dll bin/x64/WinPixEventRuntime.lib)
endfunction()
