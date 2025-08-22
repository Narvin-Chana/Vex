include(FetchContent)

# Choose URLs based on platform
if(WIN32)
    set(SLANG_RELEASE_URL "https://github.com/shader-slang/slang/releases/download/v2025.14.3/slang-2025.14.3-windows-x86_64.zip")
elseif(UNIX)
    set(SLANG_RELEASE_URL "https://github.com/shader-slang/slang/releases/download/v2025.14.3/slang-2025.14.3-linux-x86_64.tar.gz")
else()
    message(FATAL_ERROR "Unsupported platform for Slang binaries")
endif()

message(STATUS "Fetching slang...")
FetchContent_Declare(
    slang
    URL ${SLANG_RELEASE_URL}
)
FetchContent_MakeAvailable(slang)

# Create imported target for slang
set(SLANG_HEADERS_INCLUDE_NAME "include")
if(WIN32)
    set(SLANG_STATIC_LIBS 
        "${slang_SOURCE_DIR}/lib/gfx.lib"
        "${slang_SOURCE_DIR}/lib/slang-rt.lib"
        "${slang_SOURCE_DIR}/lib/slang.lib"
    )
    set(SLANG_SHARED_LIBS 
        "${slang_SOURCE_DIR}/bin/gfx.dll"
        # These are potentially not necessary, for now left as disabled.
        #"${slang_SOURCE_DIR}/bin/slang-glsl-module.dll"
        #"${slang_SOURCE_DIR}/bin/slang-glslang.dll"
        #"${slang_SOURCE_DIR}/bin/slang-llvm.dll"
        "${slang_SOURCE_DIR}/bin/slang-rt.dll"
        "${slang_SOURCE_DIR}/bin/slang.dll"
    )
elseif(UNIX)
    set(SLANG_SHARED_LIBS 
        "${slang_SOURCE_DIR}/lib/libgfx.so"
        # These are potentially not necessary, for now left as disabled.
        #"${slang_SOURCE_DIR}/lib/libslang-glsl-module.dll"
        #"${slang_SOURCE_DIR}/lib/libslang-glslang.dll"
        #"${slang_SOURCE_DIR}/lib/libslang-llvm.dll"
        "${slang_SOURCE_DIR}/lib/libslang-rt.so"
        "${slang_SOURCE_DIR}/lib/libslang.so"
    )
endif()

option(VEX_ENABLE_SLANG "Enables the possibility to use the Slang shader compiler backend." OFF)

function(build_with_slang target)
    if (VEX_ENABLE_SLANG)
        if(WIN32)
            message(STATUS "Statically linked with Slang: ${SLANG_STATIC_LIBS}")
            target_link_libraries(${target} PRIVATE "${SLANG_STATIC_LIBS}")
        elseif(UNIX)
            target_link_libraries(${target} PRIVATE "${SLANG_SHARED_LIBS}")
            set_target_properties(${target} PROPERTIES
                INSTALL_RPATH "${slang_SOURCE_DIR}"
                BUILD_WITH_INSTALL_RPATH TRUE
            )
        endif()
        message(STATUS "Installing Slang headers: ${slang_SOURCE_DIR}/${SLANG_HEADERS_INCLUDE_NAME}")
        add_header_only_dependency(${target} slang "${slang_SOURCE_DIR}" "${SLANG_HEADERS_INCLUDE_NAME}" "slang")
        target_sources(Vex PRIVATE
            "src/Vex/Shaders/SlangImpl.h"
            "src/Vex/Shaders/SlangImpl.cpp"
        )
    endif()
    target_compile_definitions(Vex PUBLIC VEX_SLANG=$<BOOL:${VEX_ENABLE_SLANG}>)
endfunction()

function(link_with_slang target)
    
endfunction()