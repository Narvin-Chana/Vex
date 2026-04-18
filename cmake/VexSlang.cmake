include(FetchContent)

if(NOT VEX_ENABLE_SHADER_COMPILER OR NOT VEX_ENABLE_SLANG)
    function(build_with_slang target)
    endfunction()
    return()
endif()

set(SLANG_VERSION "2026.7")
set(SLANG_NAME slang_${SLANG_VERSION})

# Choose URLs based on platform
if(WIN32)
    set(SLANG_RELEASE_URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-windows-x86_64.zip")
elseif(LINUX)
    set(SLANG_RELEASE_URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-linux-x86_64.tar.gz")
else()
    message(FATAL_ERROR "Unsupported platform for Slang binaries")
endif()

message(STATUS "Fetching Slang...")
FetchContent_Declare(
    ${SLANG_NAME}
    URL ${SLANG_RELEASE_URL}
)
FetchContent_MakeAvailable(${SLANG_NAME})

set(SLANG_SOURCE_DIR ${slang_${SLANG_VERSION}_SOURCE_DIR})
set(SLANG_INCLUDE_DIR "${SLANG_SOURCE_DIR}/include")

# Create imported target for slang
if(WIN32)
    set(SLANG_STATIC_LIBS
        "${SLANG_SOURCE_DIR}/lib/gfx.lib"
        "${SLANG_SOURCE_DIR}/lib/slang-rt.lib"
        "${SLANG_SOURCE_DIR}/lib/slang.lib"
    )
    set(SLANG_RUNTIME_LIBS
        "${SLANG_SOURCE_DIR}/bin/gfx.dll"
        "${SLANG_SOURCE_DIR}/bin/slang-glsl-module.dll"
        "${SLANG_SOURCE_DIR}/bin/slang-glslang.dll"
        "${SLANG_SOURCE_DIR}/bin/slang-llvm.dll"
        "${SLANG_SOURCE_DIR}/bin/slang-rt.dll"
        "${SLANG_SOURCE_DIR}/bin/slang.dll"
        "${SLANG_SOURCE_DIR}/bin/slang-compiler.dll"
    )
elseif(LINUX)
    set(DXC_STATIC_LIB "")
    set(SLANG_RUNTIME_LIBS
        "${SLANG_SOURCE_DIR}/lib/libgfx.so"
        "${SLANG_SOURCE_DIR}/lib/libslang-glsl-module-${SLANG_VERSION}.so"
        "${SLANG_SOURCE_DIR}/lib/libslang-glslang-${SLANG_VERSION}.so"
        "${SLANG_SOURCE_DIR}/lib/libslang-llvm.so"
        "${SLANG_SOURCE_DIR}/lib/libslang-rt.so"
        "${SLANG_SOURCE_DIR}/lib/libslang.so"
    )
endif()

if(NOT TARGET ${SLANG_NAME})
    add_library(${SLANG_NAME} INTERFACE IMPORTED GLOBAL)
    set_target_properties(${SLANG_NAME} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SLANG_INCLUDE_DIR}"
    )
    
    if(WIN32)
        set_target_properties(${SLANG_NAME} PROPERTIES
            INTERFACE_LINK_LIBRARIES "${SLANG_STATIC_LIBS}"
        )
    elseif(LINUX)
        set_target_properties(${SLANG_NAME} PROPERTIES
            INTERFACE_LINK_LIBRARIES "${SLANG_RUNTIME_LIBS}"
        )
    endif()
endif()

function(build_with_slang target)
    target_link_libraries(${target} PUBLIC ${SLANG_NAME})

    if(LINUX)
        set_target_properties(${target} PROPERTIES
            INSTALL_RPATH "${SLANG_SOURCE_DIR}/lib"
            BUILD_WITH_INSTALL_RPATH TRUE
        )
    endif()

    message(STATUS "Statically linked with Slang: ${SLANG_STATIC_LIBS}")
    
    add_header_only_dependency(${target} ${SLANG_NAME} "${SLANG_SOURCE_DIR}" "${SLANG_HEADERS_INCLUDE_NAME}" "slang")

    target_sources(${target} PRIVATE
        "src/ShaderCompiler/Compiler/Slang/SlangCompiler.h"
        "src/ShaderCompiler/Compiler/Slang/SlangCompiler.cpp"
    )
    
    target_compile_definitions(${target} PUBLIC VEX_SLANG=1)

    vex_add_files_to_target_property(${target} "VEX_RUNTIME_DLLS" ${SLANG_RUNTIME_LIBS})
endfunction()

function(link_with_slang target)
    message(STATUS "Linked ${target} with Slang.")
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SLANG_SHARED_LIBS}
        $<TARGET_FILE_DIR:${target}>)
    endif()
    # Copies the Vex helper shader file to the target's working directory.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VEX_ROOT_DIR}/shaders/Vex.slang"
            "$<TARGET_FILE_DIR:${target}>/Vex.slang"
    )
endfunction()