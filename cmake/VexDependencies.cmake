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

# One file per dependency, each defining _vex_resolve_<name>. They are installed next to this file.
foreach(dependency agility pix dxc slang spirv_reflect)
    include("${CMAKE_CURRENT_LIST_DIR}/dependencies/${dependency}.cmake")
endforeach()

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
