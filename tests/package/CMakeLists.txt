cmake_minimum_required(VERSION 4.2.2)
project(VexPackageTest LANGUAGES CXX C)

# Checks that an installed Vex can be consumed by another project with find_package.
find_package(Vex REQUIRED)

add_executable(vex_package_test "main.cpp")
target_link_libraries(vex_package_test PRIVATE Vex::Vex)
target_compile_definitions(vex_package_test PRIVATE SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Dummy.hlsl")
vex_setup_runtime(vex_package_test)
