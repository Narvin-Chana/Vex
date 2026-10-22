# DirectX 12 Agility SDK: headers, plus the redistributable DLLs which executables must deploy in a D3D12/ folder.
# No package manager ships it with the header layout of the NuGet package, so it is either provided through
# VEX_AGILITY_SDK_ROOT (an extracted Microsoft.Direct3D.D3D12 NuGet package) or downloaded.
function(_vex_resolve_agility)
    if(TARGET VexDeps::agility)
        return()
    endif()
    set(min_version 1.618.4)
    set(root "${VEX_AGILITY_SDK_ROOT}")
    if(NOT root AND NOT VEX_DOWNLOAD_DEPENDENCIES AND NOT DEFINED VEX_DEPS_BUNDLE_ROOT)
        message(FATAL_ERROR "Vex: VEX_DOWNLOAD_DEPENDENCIES is OFF, set VEX_AGILITY_SDK_ROOT to an extracted "
            "Microsoft.Direct3D.D3D12 NuGet package (${min_version} or later).")
    endif()
    if(NOT root OR DEFINED VEX_DEPS_BUNDLE_ROOT)
        _vex_download(agility root
            "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/${min_version}"
            e2049bccbc506a49e3f495c07ee1f97b05a1b833cf8b3db1cf93da56baf2ec53
            DOWNLOAD_NAME agility.zip
        )
    endif()

    file(STRINGS "${root}/Microsoft.Direct3D.D3D12.nuspec" package_version REGEX "<version>")
    string(REGEX REPLACE ".*<version>([0-9.]+)</version>.*" "\\1" package_version "${package_version}")
    if(NOT package_version MATCHES "^[0-9]+\\.([0-9]+)\\.")
        message(FATAL_ERROR "Unable to read the D3D12 Agility SDK version from ${root}")
    endif()
    # The SDK version exported by the executable (D3D12SDKVersion) is the minor version of the package.
    set(version ${CMAKE_MATCH_1})
    if(package_version VERSION_LESS min_version)
        message(FATAL_ERROR "The D3D12 Agility SDK in ${root} is ${package_version}, "
            "Vex requires at least ${min_version}.")
    endif()

    set(bin_dir "build/native/bin/x64")
    add_library(VexDeps::agility INTERFACE IMPORTED ${_VEX_IMPORTED_SCOPE})
    target_include_directories(VexDeps::agility INTERFACE "${root}/build/native/include")
    set_target_properties(VexDeps::agility PROPERTIES
        VEX_SDK_VERSION ${version}
        VEX_RUNTIME_FILES "${root}/${bin_dir}/D3D12Core.dll;${root}/${bin_dir}/d3d12SDKLayers.dll"
    )
    _vex_bundle(agility "${root}" Microsoft.Direct3D.D3D12.nuspec build/native/include
        ${bin_dir}/D3D12Core.dll ${bin_dir}/d3d12SDKLayers.dll)
endfunction()
