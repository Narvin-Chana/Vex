# Dependencies of Vex's examples and tests, none of these are needed to build or use the library itself.
include(FetchContent)

if(VEX_BUILD_EXAMPLES)
    # GLFW for windowing.
    set(GLFW_BUILD_DOCS OFF)
    set(GLFW_BUILD_TESTS OFF)
    set(GLFW_BUILD_EXAMPLES OFF)
    set(GLFW_INSTALL OFF)
    if(LINUX)
        set(GLFW_BUILD_WAYLAND ON)
        set(GLFW_BUILD_X11 ON)
    endif()
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        FIND_PACKAGE_ARGS NAMES glfw3
    )

    # STB Image
    FetchContent_Declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG f58f558c120e9b32c217290b80bad1a0729fbb2c
    )

    # ImGui for the ImGui example.
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.9b
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(glfw stb imgui)

    add_library(VexDev::stb INTERFACE IMPORTED GLOBAL)
    target_include_directories(VexDev::stb INTERFACE "${stb_SOURCE_DIR}")
endif()

if(VEX_BUILD_TESTS)
    set(INSTALL_GTEST OFF)
    FetchContent_Declare(GTest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        FIND_PACKAGE_ARGS NAMES GTest
    )
    FetchContent_MakeAvailable(GTest)
endif()

# RenderDoc's in-application API is a single header.
set(RENDERDOC_INCLUDE_DIR "${FETCHCONTENT_BASE_DIR}/renderdoc-v1.41")
if(NOT EXISTS "${RENDERDOC_INCLUDE_DIR}/renderdoc_app.h")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/baldurk/renderdoc/v1.41/renderdoc/api/app/renderdoc_app.h"
        "${RENDERDOC_INCLUDE_DIR}/renderdoc_app.h"
        EXPECTED_HASH SHA256=e090464ca63e63a1a087e30875a0447aad30eadae4d5bff75b6731cd1643c310
    )
endif()
add_library(VexDev::renderdoc INTERFACE IMPORTED GLOBAL)
target_include_directories(VexDev::renderdoc INTERFACE "${RENDERDOC_INCLUDE_DIR}")
