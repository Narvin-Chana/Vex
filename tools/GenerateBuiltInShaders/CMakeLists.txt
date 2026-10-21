add_executable(generate_built_in_shaders
    "GenerateBuiltInShaders.cpp"
)
target_link_libraries(generate_built_in_shaders PRIVATE Vex::Vex Vex::ShaderCompiler)
vex_setup_runtime(generate_built_in_shaders)
