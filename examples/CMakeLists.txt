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

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)
add_library(imguiLib STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
)
# Backend files
if(VEX_ENABLE_VULKAN)
    target_sources(imguiLib PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
endif()

if(VEX_ENABLE_DX12)
    target_sources(imguiLib PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp)
endif()

target_include_directories(imguiLib PUBLIC 
     ${imgui_SOURCE_DIR}
     ${imgui_SOURCE_DIR}/backends
)

# Link ImGui with its dependencies
target_link_libraries(imguiLib PUBLIC glfw)

set(EXAMPLES_COMMON_LIBS Vex imguiLib glfw)
# Find all example directories
file(GLOB EXAMPLE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/*")

foreach(EXAMPLE_DIR ${EXAMPLE_DIRS})
  if(IS_DIRECTORY ${EXAMPLE_DIR})
    message(STATUS "Detected example: ${EXAMPLE_DIR}")

    # Get the example name from the directory name
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_DIR} NAME)
    
    # Find the source files in the example dir
    file(GLOB EXAMPLE_SOURCES "${EXAMPLE_DIR}/*.cpp" "${EXAMPLE_DIR}/*.h")

    # Only create the example if there are source files.
    if (EXAMPLE_SOURCES)
        message(STATUS "Found files:")
        foreach(FILE ${EXAMPLE_SOURCES})
            # Show relative path from the include directory
            message(STATUS "  - ${FILE}")
        endforeach()
        
        add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCES} "ExampleApplication.cpp")

        # Setup the runtime dependencies of Vex
        vex_setup_runtime(${EXAMPLE_NAME})

        if(UNIX)
            link_with_dxc(${EXAMPLE_NAME})
        endif()

        # Link the example
        target_link_libraries(${EXAMPLE_NAME} PRIVATE ${EXAMPLES_COMMON_LIBS})
        target_include_directories(${EXAMPLE_NAME} PRIVATE "${EXAMPLE_DIR}" "${CMAKE_SOURCE_DIR}/src")
    endif()
  endif()
endforeach()
