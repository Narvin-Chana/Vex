# This file configures Git hooks for the project, mainly to run clang-format as a pre-commit hook.

# Only install hooks if this is the main project, not a subproject
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    message(STATUS "Setting up Git hooks for main project: ${PROJECT_NAME}")
    
    # Define the directory where hooks are stored in the repository
    set(GIT_HOOKS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.githooks")
    
    # Check if the hooks directory exists
    if(NOT EXISTS "${GIT_HOOKS_DIR}")
        message(STATUS "Creating hooks directory: ${GIT_HOOKS_DIR}")
        file(MAKE_DIRECTORY "${GIT_HOOKS_DIR}")
    endif()
    
    # Use git config to set the hooks path
    execute_process(
        COMMAND git config --local core.hooksPath .githooks/
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_CONFIG_RESULT
    )
    
    if(GIT_CONFIG_RESULT EQUAL 0)
        message(STATUS "Successfully set Git hooks path to: .githooks/")
    else()
        message(WARNING "Failed to set Git hooks path. Is this a Git repository?")
    endif()
else()
    message(STATUS "Skipping Git hooks setup for subproject: ${PROJECT_NAME}")
endif()