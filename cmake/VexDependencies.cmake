# Resolves Vex's third-party dependencies into VexDeps::<name> targets.
#
# Used in two places:
#  - Vex's own build: each dependency is looked up with find_package first (unless VEX_FIND_DEPENDENCIES is OFF),
#    and a pinned release is downloaded as a fallback (unless VEX_DOWNLOAD_DEPENDENCIES is OFF, which fails instead).
#    Downloaded copies are installed alongside Vex.
#  - VexConfig.cmake (VEX_DEPS_BUNDLE_ROOT is defined): each dependency is found again, or taken from the copy
#    installed alongside Vex.

if(DEFINED VEX_DEPS_BUNDLE_ROOT)
    set(_VEX_IMPORTED_SCOPE "")
else()
    # Vex is often consumed with add_subdirectory/FetchContent, its dependencies must be visible to the parent project.
    set(_VEX_IMPORTED_SCOPE GLOBAL)
endif()

# Wraps an installed package as VexDeps::<name>, provided it is at least VERSION (the pinned one) and one of its
# targets exposes HEADER under the spelling used by Vex's sources. Otherwise the caller falls back to the download.
function(_vex_find_dependency name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "PACKAGE;VERSION;HEADER" "TARGETS;PATH_SUFFIXES")
    set(_vex_${name}_package ${arg_PACKAGE} PARENT_SCOPE)
    if(DEFINED VEX_DEPS_BUNDLE_ROOT)
        if(NOT DEFINED VEX_DEP_${name}_BUNDLED OR VEX_DEP_${name}_BUNDLED)
            return()
        endif()
    elseif(NOT VEX_FIND_DEPENDENCIES)
        return()
    endif()

    find_package(${arg_PACKAGE} ${arg_VERSION} CONFIG QUIET ${_VEX_IMPORTED_SCOPE})
    # Packages without a version file (e.g. most vcpkg ports of Vex's dependencies) are rejected by a versioned
    # find_package, so the unversioned candidate is retried on its own: an outdated versioned one must not be picked.
    list(FIND ${arg_PACKAGE}_CONSIDERED_VERSIONS unknown unversioned)
    if(NOT ${arg_PACKAGE}_FOUND AND unversioned GREATER_EQUAL 0)
        list(GET ${arg_PACKAGE}_CONSIDERED_CONFIGS ${unversioned} config_file)
        cmake_path(GET config_file PARENT_PATH config_dir)
        find_package(${arg_PACKAGE} CONFIG QUIET ${_VEX_IMPORTED_SCOPE} PATHS "${config_dir}" NO_DEFAULT_PATH)
        if(${arg_PACKAGE}_FOUND)
            message(WARNING "Vex: ${config_file} does not report its version, it is used without checking that it is "
                "at least ${arg_VERSION}.")
        endif()
    endif()
    foreach(candidate IN LISTS arg_TARGETS)
        if(NOT TARGET ${candidate})
            continue()
        endif()
        get_target_property(include_dirs ${candidate} INTERFACE_INCLUDE_DIRECTORIES)
        unset(header_dir)
        find_path(header_dir NAMES "${arg_HEADER}" PATHS ${include_dirs} PATH_SUFFIXES ${arg_PATH_SUFFIXES}
            NO_DEFAULT_PATH NO_CACHE)
        if(header_dir)
            add_library(VexDeps::${name} INTERFACE IMPORTED ${_VEX_IMPORTED_SCOPE})
            target_link_libraries(VexDeps::${name} INTERFACE ${candidate})
            target_include_directories(VexDeps::${name} INTERFACE "${header_dir}")
            set_property(GLOBAL PROPERTY VEX_DEP_${name}_BUNDLED OFF)
            message(STATUS "Vex: using ${name} from ${candidate} (${header_dir})")
            return()
        endif()
    endforeach()

    if(DEFINED VEX_DEPS_BUNDLE_ROOT)
        message(FATAL_ERROR "Vex was built against the '${arg_PACKAGE}' package, which could not be found.")
    elseif(${arg_PACKAGE}_FOUND)
        message(STATUS "Vex: found ${arg_PACKAGE} but it does not provide <${arg_HEADER}>, it is not used.")
    elseif(${arg_PACKAGE}_CONSIDERED_CONFIGS)
        message(STATUS "Vex: ${arg_PACKAGE} older than ${arg_VERSION} found (${${arg_PACKAGE}_CONSIDERED_VERSIONS}), "
            "it is not used.")
    endif()
endfunction()

# Downloads and extracts a pinned archive, extra arguments are forwarded to FetchContent_Declare.
function(_vex_download name out_root url sha256)
    if(DEFINED VEX_DEPS_BUNDLE_ROOT)
        set(${out_root} "${VEX_DEPS_BUNDLE_ROOT}/${name}" PARENT_SCOPE)
        return()
    endif()
    if(NOT VEX_DOWNLOAD_DEPENDENCIES)
        # _vex_<name>_package is set by the caller's _vex_find_dependency.
        if(_vex_${name}_package)
            set(missing "a suitable '${_vex_${name}_package}' package")
        else()
            set(missing "${name}")
        endif()
        message(FATAL_ERROR "Vex: VEX_DOWNLOAD_DEPENDENCIES is OFF and ${missing} was not found. "
            "Make it findable (eg: through CMAKE_PREFIX_PATH), or turn VEX_DOWNLOAD_DEPENDENCIES back on.")
    endif()
    include(FetchContent)
    message(STATUS "Vex: fetching ${name}...")
    FetchContent_Declare(vex_${name} URL "${url}" URL_HASH SHA256=${sha256} ${ARGN})
    FetchContent_MakeAvailable(vex_${name})
    set(${out_root} "${vex_${name}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

# Installs the listed files/directories (relative to root) of a downloaded dependency alongside Vex.
function(_vex_bundle name root)
    if(DEFINED VEX_DEPS_BUNDLE_ROOT)
        return()
    endif()
    set_property(GLOBAL PROPERTY VEX_DEP_${name}_BUNDLED ON)
    foreach(item IN LISTS ARGN)
        cmake_path(GET item PARENT_PATH parent)
        cmake_path(APPEND VEX_INSTALL_CMAKEDIR deps ${name} ${parent} OUTPUT_VARIABLE destination)
        if(IS_DIRECTORY "${root}/${item}")
            install(DIRECTORY "${root}/${item}" DESTINATION "${destination}")
        elseif(EXISTS "${root}/${item}")
            install(FILES "${root}/${item}" DESTINATION "${destination}")
        endif()
    endforeach()
endfunction()

# Creates VexDeps::<name> from a prebuilt shared library (the import library is only needed on Windows).
function(_vex_imported_library name include_dir location)
    add_library(VexDeps::${name} SHARED IMPORTED ${_VEX_IMPORTED_SCOPE})
    set_target_properties(VexDeps::${name} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${include_dir}"
        IMPORTED_LOCATION "${location}"
    )
    if(ARGN)
        set_target_properties(VexDeps::${name} PROPERTIES IMPORTED_IMPLIB "${ARGN}")
    endif()
endfunction()

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

# SPIRV-Reflect (vcpkg: spirv-reflect). Upstream is meant to be embedded as a single source file, which is what
# happens when no package is found: VEX_SPIRV_REFLECT_DIR is then set for the shader compiler to compile it.
function(_vex_resolve_spirv_reflect)
    if(TARGET VexDeps::spirv_reflect)
        return()
    endif()
    set(version 1.4.321.0)
    _vex_find_dependency(spirv_reflect PACKAGE unofficial-spirv-reflect VERSION ${version} HEADER spirv_reflect.h
        TARGETS unofficial::spirv-reflect unofficial::spirv-reflect::spirv-reflect PATH_SUFFIXES spirv-reflect)
    if(TARGET VexDeps::spirv_reflect OR DEFINED VEX_DEPS_BUNDLE_ROOT)
        return()
    endif()
    _vex_download(spirv_reflect root
        "https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-${version}.tar.gz"
        254ed36128e7abe8fb12ef804319d0790e059093c116e9fe55d4003880515f85
        SOURCE_SUBDIR "<invalid path>" # this forces FetchContent to pull but not configure the dependency
    )
    set(VEX_SPIRV_REFLECT_DIR "${root}" PARENT_SCOPE)
endfunction()

# Expects VEX_ENABLE_DX12, VEX_ENABLE_VULKAN, VEX_ENABLE_SHADER_COMPILER, VEX_ENABLE_DXC and VEX_ENABLE_SLANG.
macro(vex_resolve_dependencies)
    if(VEX_ENABLE_DX12)
        _vex_resolve_agility()
        _vex_resolve_pix()
    endif()
    if(VEX_ENABLE_VULKAN)
        # 1.4.321 is the minimum version required due to the use of VK_KHR_unified_image_layouts.
        # If you change this version, you should also update the version used in "cmake-multi-platform.yml".
        find_package(Vulkan 1.4.321 REQUIRED ${_VEX_IMPORTED_SCOPE})
    endif()
    if(VEX_ENABLE_SHADER_COMPILER)
        if(VEX_ENABLE_DXC)
            _vex_resolve_dxc()
        endif()
        if(VEX_ENABLE_SLANG)
            _vex_resolve_slang()
        endif()
        if(VEX_ENABLE_VULKAN)
            _vex_resolve_spirv_reflect()
        endif()
    endif()
endmacro()
