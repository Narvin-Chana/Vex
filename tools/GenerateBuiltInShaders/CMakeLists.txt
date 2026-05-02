project(generate_built_in_shaders CXX)
add_executable(generate_built_in_shaders
    "GenerateBuiltInShaders.cpp"
)
target_link_libraries(generate_built_in_shaders PRIVATE Vex)
target_include_directories(generate_built_in_shaders PRIVATE "${CMAKE_SOURCE_DIR}/src")
vex_setup_runtime(generate_built_in_shaders)
