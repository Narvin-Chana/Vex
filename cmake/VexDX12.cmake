# VexDX12.cmake - DX12 backend configuration
#
# Dependencies, see the "Dependencies" section of the README for the CMake variables controlling them:
#  - WinPixEventRuntime, provides Microsoft::WinPixEventRuntime (the target name of vcpkg's winpixevent port). An
#    installed winpixevent package (at least PIX_VERSION) is used when found, otherwise the NuGet package is downloaded.
#  - The D3D12 Agility SDK, provides D3D12AgilitySDK. It is taken from the Microsoft.Direct3D.D3D12 NuGet package,
#    either downloaded or pointed to by FETCHCONTENT_SOURCE_DIR_D3D12_AGILITY_SDK.
include(FetchContent)

set(PIX_VERSION 1.0.240308001)
FetchContent_Declare(winpixevent
    URL "https://www.nuget.org/api/v2/package/WinPixEventRuntime/${PIX_VERSION}"
    URL_HASH SHA256=726acc93d6968e2146261a1e415521747d50ad69894c2b42b5d0d4c29fd66ec4
    DOWNLOAD_NAME WinPixEventRuntime.zip
    FIND_PACKAGE_ARGS ${PIX_VERSION} CONFIG GLOBAL
)

set(D3D12_AGILITY_SDK_MIN_VERSION 1.618.4)
FetchContent_Declare(d3d12_agility_sdk
    URL "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/${D3D12_AGILITY_SDK_MIN_VERSION}"
    URL_HASH SHA256=e2049bccbc506a49e3f495c07ee1f97b05a1b833cf8b3db1cf93da56baf2ec53
    DOWNLOAD_NAME Microsoft.Direct3D.D3D12.zip
)
FetchContent_MakeAvailable(winpixevent d3d12_agility_sdk)

# Not found as a package: the NuGet package was downloaded instead.
if(NOT TARGET Microsoft::WinPixEventRuntime)
    add_library(Microsoft::WinPixEventRuntime SHARED IMPORTED GLOBAL)
    set_target_properties(Microsoft::WinPixEventRuntime PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${winpixevent_SOURCE_DIR}/Include"
        IMPORTED_LOCATION "${winpixevent_SOURCE_DIR}/bin/x64/WinPixEventRuntime.dll"
        IMPORTED_IMPLIB "${winpixevent_SOURCE_DIR}/bin/x64/WinPixEventRuntime.lib"
    )
endif()

if(NOT TARGET D3D12AgilitySDK)
    # The version exported by the executable (D3D12SDKVersion) is the minor version of the NuGet package.
    file(STRINGS "${d3d12_agility_sdk_SOURCE_DIR}/Microsoft.Direct3D.D3D12.nuspec" D3D12_AGILITY_SDK_VERSION
        REGEX "<version>")
    if(NOT D3D12_AGILITY_SDK_VERSION MATCHES "<version>([0-9]+\\.([0-9]+)\\.[0-9.]+)</version>")
        message(FATAL_ERROR "Unable to read the D3D12 Agility SDK version from ${d3d12_agility_sdk_SOURCE_DIR}")
    endif()
    if(CMAKE_MATCH_1 VERSION_LESS D3D12_AGILITY_SDK_MIN_VERSION)
        message(FATAL_ERROR "The D3D12 Agility SDK in ${d3d12_agility_sdk_SOURCE_DIR} is ${CMAKE_MATCH_1}, "
            "Vex requires at least ${D3D12_AGILITY_SDK_MIN_VERSION}.")
    endif()

    set(D3D12_AGILITY_SDK_BIN_DIR "${d3d12_agility_sdk_SOURCE_DIR}/build/native/bin/x64")
    add_library(D3D12AgilitySDK INTERFACE IMPORTED GLOBAL)
    set_target_properties(D3D12AgilitySDK PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${d3d12_agility_sdk_SOURCE_DIR}/build/native/include"
    )
    # Read by vex_setup_runtime: the DLLs have to be deployed to a D3D12/ folder next to the executable, which also
    # has to export the SDK version.
    set_property(GLOBAL PROPERTY VEX_D3D12_AGILITY_SDK_VERSION ${CMAKE_MATCH_2})
    set_property(GLOBAL PROPERTY VEX_D3D12_AGILITY_SDK_DLLS
        "${D3D12_AGILITY_SDK_BIN_DIR}/D3D12Core.dll"
        "${D3D12_AGILITY_SDK_BIN_DIR}/d3d12SDKLayers.dll"
    )
endif()

function(setup_dx12_backend TARGET)
    message(STATUS "Setting up DirectX 12 backend...")

    # =========================================
    # Vex DX12 Sources
    # =========================================
    set(VEX_DX12_SOURCES 
        # DX12 RHI
        "src/DX12/DX12RHIFwd.h"
        "src/DX12/RHI/DX12Barrier.h"
        "src/DX12/RHI/DX12Barrier.cpp"
        "src/DX12/RHI/DX12Buffer.h"
        "src/DX12/RHI/DX12Buffer.cpp"
        "src/DX12/RHI/DX12RHI.h"
        "src/DX12/RHI/DX12RHI.cpp"
        "src/DX12/RHI/DX12Fence.h"
        "src/DX12/RHI/DX12Fence.cpp"
        "src/DX12/RHI/DX12CommandPool.h"
        "src/DX12/RHI/DX12CommandPool.cpp"
        "src/DX12/RHI/DX12CommandList.h"
        "src/DX12/RHI/DX12CommandList.cpp"
        "src/DX12/RHI/DX12SwapChain.h"
        "src/DX12/RHI/DX12SwapChain.cpp"
        "src/DX12/RHI/DX12Texture.h"
        "src/DX12/RHI/DX12Texture.cpp"
        "src/DX12/RHI/DX12PipelineState.h"
        "src/DX12/RHI/DX12PipelineState.cpp"
        "src/DX12/RHI/DX12ResourceLayout.h"
        "src/DX12/RHI/DX12ResourceLayout.cpp"
        "src/DX12/RHI/DX12DescriptorPool.h"
        "src/DX12/RHI/DX12DescriptorPool.cpp"
        "src/DX12/RHI/DX12Allocator.h"
        "src/DX12/RHI/DX12Allocator.cpp"
        "src/DX12/RHI/DX12ScopedGPUEvent.h"
        "src/DX12/RHI/DX12ScopedGPUEvent.cpp"
        "src/DX12/RHI/DX12TimestampQueryPool.cpp"
        "src/DX12/RHI/DX12TimestampQueryPool.h"
        "src/DX12/RHI/DX12AccelerationStructure.h"
        "src/DX12/RHI/DX12AccelerationStructure.cpp"
        "src/DX12/RHI/DX12PhysicalDevice.h"
        "src/DX12/RHI/DX12PhysicalDevice.cpp"
        # DX12 API
        "src/DX12/DX12DescriptorHeap.h"
        "src/DX12/DX12Headers.h"
        "src/DX12/DX12Formats.h" 
        "src/DX12/HRChecker.h"
        "src/DX12/HRChecker.cpp"
        "src/DX12/DXGIFactory.h"
        "src/DX12/DXGIFactory.cpp"
        "src/DX12/DX12Debug.h"
        "src/DX12/DX12Debug.cpp"
        "src/DX12/DX12Sampler.h"
        "src/DX12/DX12Sampler.cpp"
        "src/DX12/DX12GraphicsPipeline.h"
        "src/DX12/DX12GraphicsPipeline.cpp"
        "src/DX12/DX12ShaderTable.h"
        "src/DX12/DX12ShaderTable.cpp"
    )
    target_sources(${TARGET} PRIVATE ${VEX_DX12_SOURCES})

    # The Agility SDK's headers are included by Vex's DX12 headers.
    target_link_libraries(${TARGET} PUBLIC D3D12AgilitySDK d3d12 dxgi dxguid PRIVATE Microsoft::WinPixEventRuntime)

    get_property(AGILITY_SDK_VERSION GLOBAL PROPERTY VEX_D3D12_AGILITY_SDK_VERSION)
    target_compile_definitions(${TARGET} PRIVATE
        DIRECTX_AGILITY_SDK_VERSION=${AGILITY_SDK_VERSION}
        D3D12_AGILITY_SDK_ENABLED
    )

    message(STATUS "DirectX 12 backend configured successfully")
endfunction()
