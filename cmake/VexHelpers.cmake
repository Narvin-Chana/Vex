# Set of helpers for Vex's CMake files

# Function to create and link header-only libraries
function(add_header_only_dependency TARGET DEP_NAME SOURCE_DIR INCLUDE_PATH INSTALL_PATH)
    if(NOT EXISTS "${SOURCE_DIR}")
        message(STATUS WARNING " Source directory does not exist: ${SOURCE_DIR}")
        return()
    endif()

    set(FULL_INCLUDE_PATH "${SOURCE_DIR}/${INCLUDE_PATH}")
    
    # Debug: Check if include path exists
    if(NOT EXISTS "${FULL_INCLUDE_PATH}")
        message(STATUS WARNING " Include path does not exist: ${FULL_INCLUDE_PATH}")
        return()
    endif()

    # Find all header files
    file(GLOB_RECURSE HEADER_FILES 
        "${FULL_INCLUDE_PATH}/*.h"
        "${FULL_INCLUDE_PATH}/*.hpp"
        "${FULL_INCLUDE_PATH}/*.hxx"
        "${FULL_INCLUDE_PATH}/*.inl"
    )
    
    if(HEADER_FILES)
        message(STATUS "Found header files:")
        foreach(HEADER ${HEADER_FILES})
            # Show relative path from the include directory
            file(RELATIVE_PATH REL_PATH "${FULL_INCLUDE_PATH}" "${HEADER}")
            message(STATUS "  - ${REL_PATH}")
        endforeach()
    else()
        message(WARNING "No header files found in: ${FULL_INCLUDE_PATH}")
    endif()

    # Create interface library for the headers
    add_library(${TARGET}_${DEP_NAME}_headers INTERFACE)
    
    # Set include directories
    target_include_directories(${TARGET}_${DEP_NAME}_headers INTERFACE
        $<BUILD_INTERFACE:${SOURCE_DIR}/${INCLUDE_PATH}>
        $<INSTALL_INTERFACE:include/${INSTALL_PATH}>
    )
    
    # Link to main target
    target_link_libraries(${TARGET} PUBLIC ${TARGET}_${DEP_NAME}_headers)
    
    # Add to the list of targets to install
    set_property(GLOBAL APPEND PROPERTY HEADER_DEPS_TO_INSTALL ${TARGET}_${DEP_NAME}_headers)
    
    # Register header install path for later
    set_property(GLOBAL APPEND PROPERTY HEADER_DIRS_TO_INSTALL 
                "${SOURCE_DIR}/${INCLUDE_PATH}" "include/${INSTALL_PATH}")
endfunction()

function(download_and_decompress_archive ARCHIVE_URL ARCHIVE_DEST_DIR)
    set(NUGET_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/temp.zip")

    file(DOWNLOAD
        "${ARCHIVE_URL}"
        "${NUGET_DOWNLOAD_DIR}"
    )

    file(ARCHIVE_EXTRACT
        INPUT "${NUGET_DOWNLOAD_DIR}"
        DESTINATION "${ARCHIVE_DEST_DIR}"
    )

    file(REMOVE "${NUGET_DOWNLOAD_DIR}")
endfunction()

function(vex_setup_runtime TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "vex_setup_runtime: Target ${TARGET} does not exist")
        return()
    endif()
    
    get_target_property(TARGET_TYPE ${TARGET} TYPE)
    if(NOT TARGET_TYPE STREQUAL "EXECUTABLE")
        message(FATAL_ERROR "vex_setup_runtime: ${TARGET} is not an executable")
        return()
    endif()

    message(STATUS "Setting up Vex runtime dependencies for ${TARGET}...")

    # Get the list of runtime DLLs stored on Vex
    get_target_property(VEX_DLLS Vex VEX_RUNTIME_DLLS)
    if(VEX_DLLS)
        foreach(DLL_PATH ${VEX_DLLS})
            get_filename_component(DLL_NAME ${DLL_PATH} NAME)
            message(STATUS "  - Found ${DLL_PATH}")
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL_PATH}"
                    "$<TARGET_FILE_DIR:${TARGET}>/${DLL_NAME}"
                COMMENT "Copying Vex runtime dll \"${DLL_NAME}\" to ${TARGET}"
            )
        endforeach()
    else()
        message(FATAL_ERROR "No runtime DLLs found for Vex...")
    endif()

    # D3D12 Agility SDK DLLs need to be inserted into the D3D12/ subdirectory.
    get_target_property(D3D12_AGILITY_DLLS Vex VEX_D3D12_AGILITY_DLLS)
    if(D3D12_AGILITY_DLLS)
        # Create D3D12 subdirectory
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory 
                "$<TARGET_FILE_DIR:${TARGET}>/D3D12"
        )
        
        # Copy each Agility SDK DLL to the D3D12/ subdirectory
        foreach(DLL_PATH ${D3D12_AGILITY_DLLS})
            get_filename_component(DLL_NAME ${DLL_PATH} NAME)
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL_PATH}"
                    "$<TARGET_FILE_DIR:${TARGET}>/D3D12/${DLL_NAME}"
                COMMENT "Copying Agility SDK: ${DLL_NAME}"
            )
        endforeach()
        
        # Add the Agility SDK version define to the target
        get_target_property(AGILITY_VERSION Vex DIRECTX_AGILITY_SDK_VERSION)
        if(NOT AGILITY_VERSION)
            message(FATAL_ERROR "Unable to obtain the DIRECTX_AGILITY_SDK_VERSION") 
        endif()

        target_compile_definitions(${TARGET} PRIVATE 
            DIRECTX_AGILITY_SDK_VERSION=${AGILITY_VERSION}
            D3D12_AGILITY_SDK_ENABLED
        )
            
        # Add the DX12AgilitySDK.cpp source if not already added
        target_sources(${TARGET} PRIVATE
            "${VEX_ROOT_DIR}/src/DX12/DX12AgilitySDK.cpp"
        )

        message(STATUS "D3D12 Agility SDK ${DX_AGILITY_SDK_VERSION} will be deployed with ${TARGET}")
    endif()

    # Copy Vex shader files
    if(DEFINED VEX_ROOT_DIR AND EXISTS "${VEX_ROOT_DIR}/shaders")
        file(GLOB VEX_SHADER_FILES
            "${VEX_ROOT_DIR}/shaders/*.hlsl"
            "${VEX_ROOT_DIR}/shaders/*.hlsli"
            "${VEX_ROOT_DIR}/shaders/*.slang"
        )
        
        if(VEX_SHADER_FILES)
            message(STATUS "Copying Vex shader files to ${TARGET}:")
            foreach(SHADER_FILE ${VEX_SHADER_FILES})
                message(STATUS "  - Found shader ${SHADER_FILE}") 
                get_filename_component(FILE_NAME ${SHADER_FILE} NAME)
                add_custom_command(TARGET ${TARGET} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${SHADER_FILE}"
                        "$<TARGET_FILE_DIR:${TARGET}>/${FILE_NAME}"
                    COMMENT "Copying Vex shader: ${FILE_NAME}"
                )
            endforeach()
        endif()
    endif()
endfunction()

function(vex_add_files_to_target_property target PROPERTY_NAME)
    get_target_property(EXISTING_FILES ${target} ${PROPERTY_NAME})
    if (NOT EXISTING_FILES)
        set(EXISTING_FILES "")
    endif()

    list(APPEND EXISTING_FILES ${ARGN})

    set_target_properties(${target} PROPERTIES ${PROPERTY_NAME} "${EXISTING_FILES}")
endfunction()