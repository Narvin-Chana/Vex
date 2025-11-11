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
    set(DXC_SHARED_LIB "${dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll" CACHE INTERNAL "")
elseif(UNIX)
    set(DXC_HEADERS_INCLUDE_NAME "include")
    set(DXC_SHARED_LIB "${dxc_SOURCE_DIR}/lib/libdxcompiler.so")
endif()

function(vex_setup_dxc target)
    if(WIN32)
        message(STATUS "Statically linked with DXC: ${DXC_STATIC_LIB}")
        target_link_libraries(${target} PRIVATE "${DXC_STATIC_LIB}")

        # Copies the dxcompiler dll to the user's working directory (avoids the program using another one in their path).
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DXC_SHARED_LIB}"
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dxcompiler.dll"
                COMMENT "Copying DXC dll to output directory..."
        )
    elseif(UNIX)
        target_link_libraries(${target} PRIVATE "${DXC_SHARED_LIB}")
        set_target_properties(${target} PROPERTIES
            INSTALL_RPATH "${dxc_SOURCE_DIR}"
            BUILD_WITH_INSTALL_RPATH TRUE
        )
    endif()
    message(STATUS "Installing DXC headers: ${dxc_SOURCE_DIR}/${DXC_HEADERS_INCLUDE_NAME}")
    add_header_only_dependency(${target} dxc "${dxc_SOURCE_DIR}" "${DXC_HEADERS_INCLUDE_NAME}" "dxc")

    # Copies the Vex helper shader files to the target's working directory.
    file(GLOB VEX_SHADER_FILES
            "${VEX_ROOT_DIR}/shaders/*.hlsl"
            "${VEX_ROOT_DIR}/shaders/*.hlsli"
    )
    foreach(SHADER_FILE ${VEX_SHADER_FILES})
        message(STATUS "  - Found shader ${SHADER_FILE}")
        get_filename_component(FILE_NAME ${SHADER_FILE} NAME)
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SHADER_FILE}
                "${VEX_OUTPUT_SHADER_DIR}/${FILE_NAME}"
                COMMENT "Copying Vex shader ${FILE_NAME}"
        )
    endforeach()
endfunction()
