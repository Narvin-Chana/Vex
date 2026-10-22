# Slang: its releases (and the vcpkg shader-slang port) ship a CMake package, so the download is consumed through it.
function(_vex_resolve_slang)
    if(TARGET VexDeps::slang)
        return()
    endif()
    set(version 2026.7)
    _vex_find_dependency(slang PACKAGE slang VERSION ${version} HEADER slang.h TARGETS slang::slang)
    if(NOT TARGET VexDeps::slang)
        set(release "https://github.com/shader-slang/slang/releases/download/v${version}/slang-${version}")
        if(WIN32)
            _vex_download(slang root "${release}-windows-x86_64.zip"
                41991aaac6bd84c61a59075f65cc3829a347dc9765879df346286c24f9623a8d)
        elseif(LINUX)
            _vex_download(slang root "${release}-linux-x86_64.tar.gz"
                4cd59c9b0e5bd192e68728fabb741095befe1a693ba699454a305aaa8c1f2bc1)
        else()
            message(FATAL_ERROR "Unsupported platform for Slang binaries")
        endif()
        find_package(slang CONFIG REQUIRED ${_VEX_IMPORTED_SCOPE} PATHS "${root}" NO_DEFAULT_PATH)
        # Otherwise the next configure would "find" the download through the cache, and stop installing it.
        unset(slang_DIR CACHE)
        add_library(VexDeps::slang INTERFACE IMPORTED ${_VEX_IMPORTED_SCOPE})
        target_link_libraries(VexDeps::slang INTERFACE slang::slang)
        _vex_bundle(slang "${root}" bin lib include cmake)
    endif()

    if(WIN32)
        # DLLs which slang.dll loads at runtime, they are not link dependencies so they have to be deployed explicitly.
        get_target_property(location slang::slang LOCATION)
        cmake_path(GET location PARENT_PATH bin_dir)
        file(GLOB runtime_files "${bin_dir}/slang-compiler.dll" "${bin_dir}/slang-glslang*.dll"
            "${bin_dir}/slang-glsl-module*.dll" "${bin_dir}/slang-llvm.dll")
        set_target_properties(VexDeps::slang PROPERTIES VEX_RUNTIME_FILES "${runtime_files}")
    endif()
endfunction()
