add_executable(generate_built_in_shaders
    "GenerateBuiltInShaders.cpp"
)
target_include_directories(generate_built_in_shaders PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(generate_built_in_shaders PRIVATE Vex)
vex_setup_runtime(generate_built_in_shaders)
