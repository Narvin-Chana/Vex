# Deploys everything an executable using Vex needs next to it: the DLLs of Vex's dependencies, the D3D12 Agility SDK
# and Vex's helper shaders.
function(vex_setup_runtime TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "vex_setup_runtime: Target ${TARGET} does not exist")
    endif()
    get_target_property(TARGET_TYPE ${TARGET} TYPE)
    if(NOT TARGET_TYPE STREQUAL "EXECUTABLE")
        message(FATAL_ERROR "vex_setup_runtime: ${TARGET} is not an executable")
    endif()

    set(OUTPUT_DIR "$<TARGET_FILE_DIR:${TARGET}>")

    # Shared libraries the executable links to (DXC, Slang, WinPixEventRuntime...), empty outside of Windows.
    set(LINKED_DLLS "$<TARGET_RUNTIME_DLLS:${TARGET}>")
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND "$<$<BOOL:${LINKED_DLLS}>:${CMAKE_COMMAND};-E;copy_if_different;${LINKED_DLLS};${OUTPUT_DIR}>"
        COMMAND_EXPAND_LISTS
        COMMENT "Copying Vex runtime dlls to ${TARGET}"
    )

    # Files which are loaded at runtime without being linked: Slang's modules and Vex's helper shaders.
    set(RUNTIME_FILES "")
    if(TARGET VexDeps::slang)
        get_target_property(SLANG_RUNTIME_FILES VexDeps::slang VEX_RUNTIME_FILES)
        if(SLANG_RUNTIME_FILES)
            list(APPEND RUNTIME_FILES ${SLANG_RUNTIME_FILES})
        endif()
    endif()
    if(TARGET Vex::ShaderCompiler AND NOT VEX_USE_EMBEDDED_SHADERS)
        get_property(SHADER_DIR GLOBAL PROPERTY VEX_SHADER_DIR)
        file(GLOB VEX_SHADER_FILES "${SHADER_DIR}/*.hlsl" "${SHADER_DIR}/*.hlsli" "${SHADER_DIR}/*.slang")
        list(APPEND RUNTIME_FILES ${VEX_SHADER_FILES})
    endif()
    if(RUNTIME_FILES)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RUNTIME_FILES} "${OUTPUT_DIR}"
            COMMENT "Copying Vex runtime files to ${TARGET}"
        )
    endif()

    # The D3D12 Agility SDK DLLs must live in a D3D12/ subdirectory, and the executable itself has to export the SDK
    # version (DX12 is loaded dynamically at startup and only looks at the main application for it).
    if(TARGET VexDeps::agility)
        get_target_property(AGILITY_DLLS VexDeps::agility VEX_RUNTIME_FILES)
        get_target_property(AGILITY_VERSION VexDeps::agility VEX_SDK_VERSION)
        get_property(AGILITY_SOURCE GLOBAL PROPERTY VEX_AGILITY_SDK_SOURCE)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}/D3D12"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${AGILITY_DLLS} "${OUTPUT_DIR}/D3D12"
            COMMENT "Copying the D3D12 Agility SDK to ${TARGET}"
        )
        target_sources(${TARGET} PRIVATE "${AGILITY_SOURCE}")
        target_compile_definitions(${TARGET} PRIVATE
            DIRECTX_AGILITY_SDK_VERSION=${AGILITY_VERSION}
            D3D12_AGILITY_SDK_ENABLED
        )
    endif()
endfunction()
