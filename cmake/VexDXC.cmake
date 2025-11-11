include(FetchContent)

# Choose URLs based on platform
if(WIN32)
    set(DXC_RELEASE_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1/dxc_2025_07_14.zip")
elseif(UNIX)
    set(DXC_RELEASE_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1/linux_dxc_2025_07_14.x86_64.tar.gz")
else()
    message(FATAL_ERROR "Unsupported platform for DXC binaries")
endif()

message(STATUS "Fetching DXC...")
FetchContent_Declare(
    dxc
    URL ${DXC_RELEASE_URL}
)
FetchContent_MakeAvailable(dxc)

# Create imported target for dxc
if(WIN32)
    set(DXC_HEADERS_INCLUDE_NAME "inc")
    set(DXC_STATIC_LIB "${dxc_SOURCE_DIR}/lib/x64/dxcompiler.lib")
    set(DXC_RUNTIME_LIB "${dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll")
elseif(UNIX)
    set(DXC_HEADERS_INCLUDE_NAME "include")
    set(DXC_STATIC_LIB "")
    set(DXC_RUNTIME_LIB "${dxc_SOURCE_DIR}/lib/libdxcompiler.so")
endif()
set(DXC_INCLUDE_DIR "${dxc_SOURCE_DIR}/${DXC_HEADERS_INCLUDE_NAME}")

if(NOT TARGET dxc::dxc)
    add_library(dxc::dxc SHARED IMPORTED GLOBAL)
    set_target_properties(dxc::dxc PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${DXC_INCLUDE_DIR}"
        IMPORTED_LOCATION "${DXC_RUNTIME_LIB}"
    )
    
    # Windows needs the import library
    if(WIN32)
        set_target_properties(dxc::dxc PROPERTIES
            IMPORTED_IMPLIB "${DXC_STATIC_LIB}"
        )
    endif()
endif()

function(build_with_dxc target)
    target_link_libraries(${target} PRIVATE dxc::dxc)
    
    if(UNIX)
        # Set RPATH for Linux builds
        set_target_properties(${target} PROPERTIES
            INSTALL_RPATH "${dxc_SOURCE_DIR}/lib"
            BUILD_WITH_INSTALL_RPATH TRUE
        )
    endif()
    
    message(STATUS "Statically linked ${target} with DXC: ${DXC_STATIC_LIB}")
    
    # Register the DXC headers for installation
    add_header_only_dependency(${target} dxc "${dxc_SOURCE_DIR}" "${DXC_HEADERS_INCLUDE_NAME}" "dxc")

    # Now store DXC's dlls in Vex's runtime dlls target property.
    vex_add_files_to_target_property(${target} "VEX_RUNTIME_DLLS" ${DXC_RUNTIME_LIB})
endfunction()
