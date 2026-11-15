# Set of helpers for Vex's CMake files, installed alongside Vex for projects using it.

# Deploys everything an executable using Vex needs next to it at runtime:
#  - the DLLs of Vex's dependencies (DXC, Slang, WinPixEventRuntime...) and the modules Slang loads at runtime,
#  - the D3D12 Agility SDK (DLLs in a D3D12/ subdirectory, and the SDK version exported by the executable),
#  - Vex's helper shaders, unless they are embedded in Vex.
# On Linux, shared libraries are instead found through the RPATH CMake gives the executable.
function(vex_setup_runtime TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "vex_setup_runtime: Target ${TARGET} does not exist")
    endif()
    get_target_property(TARGET_TYPE ${TARGET} TYPE)
    if(NOT TARGET_TYPE STREQUAL "EXECUTABLE")
        message(FATAL_ERROR "vex_setup_runtime: ${TARGET} is not an executable")
    endif()

    set(OUTPUT_DIR "$<TARGET_FILE_DIR:${TARGET}>")

    # Shared libraries the executable links to, directly or through Vex (always empty outside of Windows).
    set(LINKED_DLLS "$<TARGET_RUNTIME_DLLS:${TARGET}>")
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND "$<$<BOOL:${LINKED_DLLS}>:${CMAKE_COMMAND};-E;copy_if_different;${LINKED_DLLS};${OUTPUT_DIR}>"
        COMMAND_EXPAND_LISTS
        COMMENT "Copying Vex runtime dlls to ${TARGET}"
    )

    # Files which are loaded at runtime without being linked to.
    get_property(RUNTIME_FILES GLOBAL PROPERTY VEX_RUNTIME_FILES)
    if(NOT VEX_USE_EMBEDDED_SHADERS)
        file(GLOB VEX_SHADER_FILES
            "${VEX_ROOT_DIR}/shaders/*.hlsl"
            "${VEX_ROOT_DIR}/shaders/*.hlsli"
            "${VEX_ROOT_DIR}/shaders/*.slang"
        )
        list(APPEND RUNTIME_FILES ${VEX_SHADER_FILES})
    endif()
    list(REMOVE_DUPLICATES RUNTIME_FILES)
    if(RUNTIME_FILES)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RUNTIME_FILES} "${OUTPUT_DIR}"
            COMMENT "Copying Vex runtime files to ${TARGET}"
        )
    endif()

    # DX12 is loaded dynamically at startup and only looks at the main executable for the Agility SDK version and
    # path, which is why DX12AgilitySDK.cpp is compiled into it.
    get_property(AGILITY_SDK_DLLS GLOBAL PROPERTY VEX_D3D12_AGILITY_SDK_DLLS)
    if(AGILITY_SDK_DLLS)
        get_property(AGILITY_SDK_VERSION GLOBAL PROPERTY VEX_D3D12_AGILITY_SDK_VERSION)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}/D3D12"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${AGILITY_SDK_DLLS} "${OUTPUT_DIR}/D3D12"
            COMMENT "Copying the D3D12 Agility SDK to ${TARGET}"
        )
        target_sources(${TARGET} PRIVATE "${VEX_ROOT_DIR}/src/DX12/DX12AgilitySDK.cpp")
        target_compile_definitions(${TARGET} PRIVATE
            DIRECTX_AGILITY_SDK_VERSION=${AGILITY_SDK_VERSION}
            D3D12_AGILITY_SDK_ENABLED
        )
    endif()
endfunction()
