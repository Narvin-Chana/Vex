option(VEX_USE_EMBEDDED_SHADERS "Whether Vex should embed helper shaders in its binary instead of distributing them in the bin folder." OFF)

if (VEX_USE_EMBEDDED_SHADERS)

    set(_vex_shader_deps "")
    if (VEX_ENABLE_DXC)
        list(APPEND _vex_shader_deps "${VEX_ROOT_DIR}/shaders/Vex.hlsli")
    endif()
    if (VEX_ENABLE_SLANG)
        list(APPEND _vex_shader_deps "${VEX_ROOT_DIR}/shaders/Vex.slang")
    endif()

    # Re-configure when any shader source changes
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_vex_shader_deps})

    set(_vex_embedded_header "#pragma once\n\n#include <string_view>\n\nnamespace vex::embedded\n{\n\n")

    if (VEX_ENABLE_DXC)
        file(READ "${VEX_ROOT_DIR}/shaders/Vex.hlsli" _vex_hlsl_src)
        string(APPEND _vex_embedded_header
            "inline constexpr const std::string_view VexHLSL = R\"VEX_SHADER_DELIM(\n${_vex_hlsl_src}\n)VEX_SHADER_DELIM\";\n\n")
    endif()

    if (VEX_ENABLE_SLANG)
        file(READ "${VEX_ROOT_DIR}/shaders/Vex.slang" _vex_slang_src)
        string(APPEND _vex_embedded_header
            "inline constexpr const std::string_view VexSlang = R\"VEX_SHADER_DELIM(\n${_vex_slang_src}\n)VEX_SHADER_DELIM\";\n\n")
    endif()

    string(APPEND _vex_embedded_header "} // namespace vex::embedded\n")

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gen/ShaderCompiler/Generated")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gen/ShaderCompiler/Generated/EmbeddedShaders.h" "${_vex_embedded_header}")

    target_include_directories(Vex PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/gen>"
    $<INSTALL_INTERFACE:include/vex/ShaderCompiler/Generated>)

    target_compile_definitions(Vex PRIVATE VEX_EMBEDDED_SHADERS=1)
else()
    target_compile_definitions(Vex PRIVATE VEX_EMBEDDED_SHADERS=0)
endif()
