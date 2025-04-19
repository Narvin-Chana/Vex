include(FetchContent)

# Force glfw to use static lib
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

set(EXAMPLES_COMMON_LIBS PRIVATE Vex glfw)

# Find all example directories
file(GLOB EXAMPLE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/*")

foreach(EXAMPLE_DIR ${EXAMPLE_DIRS})
  if(IS_DIRECTORY ${EXAMPLE_DIR})
    message(STATUS "Detected example: ${EXAMPLE_DIR}")

    # Get the example name from the directory name
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_DIR} NAME)
    
    # Find the source files in the example dir
    file(GLOB EXAMPLE_SOURCES "${EXAMPLE_DIR}/*.cpp")

    # Only create the example if there are source files.
    if (EXAMPLE_SOURCES)
        add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCES} "ExampleApplication.h" "ExampleApplication.cpp")

        # Setup the d3d12 agility sdk
        vex_setup_d3d12_agility_runtime(${EXAMPLE_NAME})

        target_link_libraries(${EXAMPLE_NAME} PRIVATE ${EXAMPLES_COMMON_LIBS})
        target_include_directories(${EXAMPLE_NAME} PRIVATE "${EXAMPLE_DIR}" "${CMAKE_SOURCE_DIR}/src")
    endif()
  endif()
endforeach()
