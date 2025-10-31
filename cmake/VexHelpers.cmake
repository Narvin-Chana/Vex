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