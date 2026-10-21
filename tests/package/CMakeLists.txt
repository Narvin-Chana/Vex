cmake_minimum_required(VERSION 4.2.2)
project(VexPackageTest LANGUAGES CXX C)

# Checks that Vex can be consumed by another project: from an install (find_package, the default) or as a
# subdirectory when VEX_SOURCE_DIR is set.
if(VEX_SOURCE_DIR)
    add_subdirectory("${VEX_SOURCE_DIR}" vex)
else()
    find_package(Vex REQUIRED COMPONENTS ShaderCompiler)
endif()

add_executable(vex_package_test "main.cpp")
target_link_libraries(vex_package_test PRIVATE Vex::Vex Vex::ShaderCompiler)
target_compile_definitions(vex_package_test PRIVATE SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Dummy.hlsl")
vex_setup_runtime(vex_package_test)
