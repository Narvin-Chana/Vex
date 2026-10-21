# ImGui does not ship a build system, compile it along with the backends the examples use.
add_library(imguiLib STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    # ImGui Backend files (specific to graphics APIs)
    "$<$<BOOL:${VEX_ENABLE_VULKAN}>:${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp>"
    "$<$<BOOL:${VEX_ENABLE_DX12}>:${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp>"
)
target_include_directories(imguiLib PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
target_link_libraries(imguiLib PUBLIC glfw Vex::Vex)

# Everything shared by all examples.
add_library(VexExamplesCommon INTERFACE)
target_sources(VexExamplesCommon INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/ExampleApplication.cpp"
)
target_include_directories(VexExamplesCommon INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(VexExamplesCommon INTERFACE Vex::Vex Vex::ShaderCompiler imguiLib glfw VexDev::stb)

# Each directory containing sources is an example.
file(GLOB EXAMPLE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(EXAMPLE_DIR ${EXAMPLE_DIRS})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_DIR} NAME)
    file(GLOB EXAMPLE_SOURCES "${EXAMPLE_DIR}/*.cpp" "${EXAMPLE_DIR}/*.h")
    if(NOT IS_DIRECTORY ${EXAMPLE_DIR} OR NOT EXAMPLE_SOURCES)
        continue()
    endif()
    # PIX events are only available with DX12.
    if(EXAMPLE_NAME STREQUAL "pix_events" AND NOT VEX_ENABLE_DX12)
        continue()
    endif()
    message(STATUS "Detected example: ${EXAMPLE_NAME}")

    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCES})
    target_include_directories(${EXAMPLE_NAME} PRIVATE "${EXAMPLE_DIR}")
    target_link_libraries(${EXAMPLE_NAME} PRIVATE VexExamplesCommon)
    if(EXAMPLE_NAME STREQUAL "pix_events")
        target_link_libraries(${EXAMPLE_NAME} PRIVATE VexDeps::pix)
    elseif(EXAMPLE_NAME STREQUAL "render_doc")
        target_link_libraries(${EXAMPLE_NAME} PRIVATE VexDev::renderdoc)
    endif()

    # Setup the runtime dependencies of Vex
    vex_setup_runtime(${EXAMPLE_NAME})
endforeach()
