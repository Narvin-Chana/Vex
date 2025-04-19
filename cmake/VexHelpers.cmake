# Set of helpers for Vex's CMake files

# Function to create and link header-only libraries
function(add_header_only_dependency TARGET DEP_NAME SOURCE_DIR INCLUDE_PATH INSTALL_PATH)
    # Create interface library for the headers
    add_library(${TARGET}_${DEP_NAME}_headers INTERFACE)
    
    # Set include directories
    target_include_directories(${TARGET}_${DEP_NAME}_headers INTERFACE
        $<BUILD_INTERFACE:${SOURCE_DIR}/${INCLUDE_PATH}>
        $<INSTALL_INTERFACE:include/${INSTALL_PATH}>
    )

    set(DEBUG_OUTPUT_HEADER_ONLY_DEP_FOUND_FILES TRUE)

    if(DEBUG_OUTPUT_HEADER_ONLY_DEP_FOUND_FILES)
        # List all header files found in the include path
        set(FULL_INCLUDE_PATH "${SOURCE_DIR}/${INCLUDE_PATH}")
        file(GLOB_RECURSE HEADER_FILES
            RELATIVE "${FULL_INCLUDE_PATH}"
            "${FULL_INCLUDE_PATH}/**.h"
            "${FULL_INCLUDE_PATH}/**.hpp"
        )

        if(HEADER_FILES)
            message(STATUS "[${DEP_NAME}] Found headers in ${FULL_INCLUDE_PATH}:")
            foreach(header ${HEADER_FILES})
                message(STATUS "  - ${header}")
            endforeach()
        else()
            message(WARNING "[${DEP_NAME}] No headers found in ${FULL_INCLUDE_PATH}")
        endif()
    endif()

    # Link to main target
    target_link_libraries(${TARGET} PUBLIC ${TARGET}_${DEP_NAME}_headers)
    
    # Add to the list of targets to install
    set_property(GLOBAL APPEND PROPERTY HEADER_DEPS_TO_INSTALL ${TARGET}_${DEP_NAME}_headers)
    
    # Register header install path for later
    set_property(GLOBAL APPEND PROPERTY HEADER_DIRS_TO_INSTALL 
                "${SOURCE_DIR}/${INCLUDE_PATH}" "include/${INSTALL_PATH}")
endfunction()
