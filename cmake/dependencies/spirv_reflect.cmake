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
