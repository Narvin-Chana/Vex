include(FetchContent)

# Choose URLs based on platform
if(WIN32)
    set(DXC_RELEASE_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1/dxc_2025_07_14.zip")
elseif(UNIX)
    set(DXC_RELEASE_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1/linux_dxc_2025_07_14.x86_64.tar.gz")
else()
    message(FATAL_ERROR "Unsupported platform for DXC binaries")
endif()

message(STATUS "Fetching dxc...")
FetchContent_Declare(
    dxc
    URL ${DXC_RELEASE_URL}
)
FetchContent_MakeAvailable(dxc)

# Create imported target for dxc
if(WIN32)
    set(DXC_HEADERS_INCLUDE_NAME "inc")
    set(DXC_STATIC_LIB "${dxc_SOURCE_DIR}/lib/x64/dxcompiler.lib")
elseif(UNIX)
    set(DXC_HEADERS_INCLUDE_NAME "include")
    set(DXC_SHARED_LIB "${dxc_SOURCE_DIR}/lib/libdxcompiler.so")
endif()

function(build_with_dxc target)
    if(WIN32)
        message(STATUS "Statically linked with DXC: ${DXC_STATIC_LIB}")
        target_link_libraries(${target} PRIVATE "${DXC_STATIC_LIB}")
    elseif(UNIX)
        target_link_libraries(${target} PRIVATE "${DXC_SHARED_LIB}")
        set_target_properties(${target} PROPERTIES
            INSTALL_RPATH "${dxc_SOURCE_DIR}"
            BUILD_WITH_INSTALL_RPATH TRUE
        )
    endif()
    message(STATUS "Installing DXC headers: ${dxc_SOURCE_DIR}/${DXC_HEADERS_INCLUDE_NAME}")
    add_header_only_dependency(${target} dxc "${dxc_SOURCE_DIR}" "${DXC_HEADERS_INCLUDE_NAME}" "dxc")
endfunction()

function(link_with_dxc target)
    message(STATUS "Linked ${target} with DirectXCompiler.")
    # Copies the Vex helper shader file to the target's working directory.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VEX_ROOT_DIR}/shaders/Vex.hlsli"
            "$<TARGET_FILE_DIR:${target}>/Vex.hlsli"
    )
endfunction()
