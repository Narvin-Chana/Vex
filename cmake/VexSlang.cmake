# Slang shader compiler, provides slang::slang.
#
# An installed slang package (at least SLANG_VERSION) is used when found, otherwise the pinned release is downloaded.
# See the "Dependencies" section of the README for the CMake variables controlling this.
include(FetchContent)

if(NOT VEX_ENABLE_SHADER_COMPILER OR NOT VEX_ENABLE_SLANG)
    function(build_with_slang target)
    endfunction()
    return()
endif()

set(SLANG_VERSION 2026.18)
set(SLANG_RELEASE_URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}")
if(WIN32)
    set(SLANG_URL "${SLANG_RELEASE_URL}-windows-x86_64.zip")
    set(SLANG_SHA256 6ffa4827b519fd0a85b38407049d87ab0c1f045fe2289cb1e6831f965169f8a1)
elseif(LINUX)
    set(SLANG_URL "${SLANG_RELEASE_URL}-linux-x86_64.tar.gz")
    set(SLANG_SHA256 03acde8d19ad17f7b94312078ed945bed554a220feff084898a1bc94a9c998f5)
else()
    message(FATAL_ERROR "Unsupported platform for Slang binaries")
endif()

FetchContent_Declare(slang
    URL "${SLANG_URL}"
    URL_HASH SHA256=${SLANG_SHA256}
    FIND_PACKAGE_ARGS ${SLANG_VERSION} CONFIG GLOBAL
)
FetchContent_MakeAvailable(slang)

# Not found as a package: the release was downloaded instead, and it ships its own CMake package. Every find_package()
# call first looks in CMAKE_FIND_PACKAGE_REDIRECTS_DIR, where FetchContent lets projects provide config files for the
# content they populate: make find_package(slang) load the downloaded package.
if(NOT TARGET slang::slang)
    if(WIN32)
        set(SLANG_CONFIG_DIR "${slang_SOURCE_DIR}/cmake")
    else()
        set(SLANG_CONFIG_DIR "${slang_SOURCE_DIR}/lib/cmake/slang")
    endif()
    foreach(SLANG_CONFIG_FILE slangConfig slangConfigVersion)
        string(REPLACE "slangConfig" "slang-config" SLANG_REDIRECT_FILE ${SLANG_CONFIG_FILE})
        file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/${SLANG_REDIRECT_FILE}.cmake"
            "include(\"${SLANG_CONFIG_DIR}/${SLANG_CONFIG_FILE}.cmake\")\n")
    endforeach()
    find_package(slang ${SLANG_VERSION} REQUIRED GLOBAL)
endif()

# Slang loads these modules at runtime, they are not link dependencies so vex_setup_runtime copies them explicitly.
# Elsewhere, they are found next to the Slang shared library.
if(WIN32)
    foreach(SLANG_MODULE slang::slang-glslang slang::slang-glsl-module slang::slang-llvm)
        if(TARGET ${SLANG_MODULE})
            set_property(GLOBAL APPEND PROPERTY VEX_RUNTIME_FILES "$<TARGET_FILE:${SLANG_MODULE}>")
        endif()
    endforeach()
endif()

function(build_with_slang target)
    # Public: Slang's headers are part of Vex's ShaderCompiler headers.
    target_link_libraries(${target} PUBLIC slang::slang)
    target_sources(${target} PRIVATE
        "src/ShaderCompiler/Compiler/Slang/SlangCompiler.h"
        "src/ShaderCompiler/Compiler/Slang/SlangCompiler.cpp"
    )
endfunction()
