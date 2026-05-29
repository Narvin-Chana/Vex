include(FetchContent)

if(NOT VEX_ENABLE_SHADER_COMPILER OR NOT VEX_ENABLE_DXC)
    function(build_with_dxc target)
    endfunction()
    return()
endif()

# Choose URLs based on platform
set(DXC_VERSION "2026_02_20")
set(DXC_NAME dxc_${DXC_VERSION})
if(WIN32)
    set(DXC_RELEASE_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/dxc_${DXC_VERSION}.zip")
elseif(LINUX)
    set(DXC_RELEASE_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/linux_dxc_${DXC_VERSION}.x86_64.tar.gz")
else()
    message(FATAL_ERROR "Unsupported platform for DXC binaries")
endif()

message(STATUS "Fetching DXC...")
FetchContent_Declare(
    ${DXC_NAME}
    URL ${DXC_RELEASE_URL}
)
FetchContent_MakeAvailable(${DXC_NAME})

set(DXC_SOURCE_DIR ${dxc_${DXC_VERSION}_SOURCE_DIR})

# Create imported target for dxc
if(WIN32)
    set(DXC_HEADERS_INCLUDE_NAME "inc")
    set(DXC_STATIC_LIB "${DXC_SOURCE_DIR}/lib/x64/dxcompiler.lib")
    set(DXC_RUNTIME_LIB "${DXC_SOURCE_DIR}/bin/x64/dxcompiler.dll")
elseif(LINUX)
    set(DXC_HEADERS_INCLUDE_NAME "include")
    set(DXC_STATIC_LIB "")
    set(DXC_RUNTIME_LIB "${DXC_SOURCE_DIR}/lib/libdxcompiler.so")
endif()
set(DXC_INCLUDE_DIR "${DXC_SOURCE_DIR}/${DXC_HEADERS_INCLUDE_NAME}")

if(NOT TARGET ${DXC_NAME})
    add_library(${DXC_NAME} SHARED IMPORTED GLOBAL)
    set_target_properties(${DXC_NAME} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${DXC_INCLUDE_DIR}"
        IMPORTED_LOCATION "${DXC_RUNTIME_LIB}"
    )
    
    # Windows needs the import library
    if(WIN32)
        set_target_properties(${DXC_NAME} PROPERTIES
            IMPORTED_IMPLIB "${DXC_STATIC_LIB}"
        )
    endif()
endif()

function(build_with_dxc target)
    target_link_libraries(${target} PRIVATE ${DXC_NAME})
    
    if(LINUX)
        # Set RPATH for Linux builds
        set_target_properties(${target} PROPERTIES
            INSTALL_RPATH "${DXC_SOURCE_DIR}/lib"
            BUILD_WITH_INSTALL_RPATH TRUE
        )
    else()
        message(STATUS "Statically linked ${target} with DXC: ${DXC_STATIC_LIB}")
    endif()

    # Register the DXC headers for installation
    add_header_only_dependency(${target} ${DXC_NAME} "${DXC_SOURCE_DIR}" "${DXC_HEADERS_INCLUDE_NAME}" "dxc")

    target_sources(${target} PRIVATE
            "src/ShaderCompiler/Compiler/DXC/DXCCompiler.h"
            "src/ShaderCompiler/Compiler/DXC/DXCCompiler.cpp"
    )

    # Now store DXC's dlls in Vex's runtime dlls target property.
    vex_add_files_to_target_property(${target} "VEX_RUNTIME_DLLS" ${DXC_RUNTIME_LIB})
endfunction()
