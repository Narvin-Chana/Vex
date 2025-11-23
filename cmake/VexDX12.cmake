# VexDX12.cmake - DX12 backend configuration
include(VexHelpers)

function(setup_pix_3 TARGET)
    # =========================================
    # PIX Events Runtime
    # =========================================

    set(PIX_EVENTS_DIR "${FETCHCONTENT_BASE_DIR}/PixEvents")
    if (NOT EXISTS "${PIX_EVENTS_DIR}/bin")
        download_and_decompress_archive(
                "https://www.nuget.org/api/v2/package/WinPixEventRuntime/1.0.240308001"
                "${PIX_EVENTS_DIR}"
        )
    endif()

    set(PIX_INCLUDE_DIR "${PIX_EVENTS_DIR}/include")
    set(PIX_STATIC_LIB "${PIX_EVENTS_DIR}/bin/x64/WinPixEventRuntime.lib")
    set(PIX_RUNTIME_DLL "${PIX_EVENTS_DIR}/bin/x64/WinPixEventRuntime.dll")

    # Create PIX imported target
    if(NOT TARGET WinPixEventRuntime::WinPixEventRuntime)
        add_library(WinPixEventRuntime::WinPixEventRuntime SHARED IMPORTED GLOBAL)
        set_target_properties(WinPixEventRuntime::WinPixEventRuntime PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${PIX_INCLUDE_DIR}"
                IMPORTED_IMPLIB "${PIX_STATIC_LIB}"
                IMPORTED_LOCATION "${PIX_RUNTIME_DLL}"
        )
    endif()

    target_link_libraries(${TARGET} PRIVATE WinPixEventRuntime::WinPixEventRuntime)
endfunction()

function(setup_dx12_backend TARGET)
    message(STATUS "Setting up DirectX 12 backend...")

    setup_pix_3(${TARGET})
    
    # =========================================
    # DirectX 12 Agility SDK
    # =========================================

    # Fetch DX12 Agility SDK
    set(DX_AGILITY_VERSION "618")
    set(AGILITY_SDK_DIR "${FETCHCONTENT_BASE_DIR}/DirectX-AgilitySDK-${DX_AGILITY_VERSION}")

    # Download the NuGet package if not already downloaded
    if(NOT EXISTS "${AGILITY_SDK_DIR}/build")
        message(STATUS "Downloading DX12 Agility SDK 1.${DX_AGILITY_VERSION}.1...")
        download_and_decompress_archive(
            "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.${DX_AGILITY_VERSION}.1"
            "${AGILITY_SDK_DIR}"
        )
        message(STATUS "DirectX Agility SDK extracted to: ${AGILITY_SDK_DIR}")
    endif()

    set(DX_AGILITY_SDK_SOURCE_DIR "${AGILITY_SDK_DIR}/build/native")
    set(DX_AGILITY_INCLUDE_DIR "${DX_AGILITY_SDK_SOURCE_DIR}/include")
    set(DX_AGILITY_BIN_DIR "${DX_AGILITY_SDK_SOURCE_DIR}/bin/x64")

    set(DX_AGILITY_RUNTIME_DLLS
        "${DX_AGILITY_BIN_DIR}/D3D12Core.dll"
        "${DX_AGILITY_BIN_DIR}/d3d12SDKLayers.dll"
    )

    if(NOT EXISTS "${DX_AGILITY_BIN_DIR}/D3D12Core.dll")
        message(FATAL_ERROR "Missing D3D12Core.dll in Agility SDK directory: ${DX_AGILITY_BIN_DIR}")
    endif()

    # Store version for downstream use
    set(DX_AGILITY_SDK_VERSION ${DX_AGILITY_VERSION} CACHE INTERNAL "DirectX Agility SDK Version")

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
        "src/DX12/RHI/Dx12TimestampQueryPool.cpp"
        "src/DX12/RHI/Dx12TimestampQueryPool.h"
        # DX12 API
        "src/DX12/DX12DescriptorHeap.h"
        "src/DX12/DX12Headers.h"
        "src/DX12/DX12Formats.h" 
        "src/DX12/DX12FeatureChecker.h"
        "src/DX12/DX12FeatureChecker.cpp"
        "src/DX12/HRChecker.h"
        "src/DX12/HRChecker.cpp"
        "src/DX12/DXGIFactory.h"
        "src/DX12/DXGIFactory.cpp"
        "src/DX12/DX12PhysicalDevice.h"
        "src/DX12/DX12PhysicalDevice.cpp"
        "src/DX12/DX12Debug.h"
        "src/DX12/DX12Debug.cpp"
        "src/DX12/DX12TextureSampler.h"
        "src/DX12/DX12TextureSampler.cpp"
        "src/DX12/DX12GraphicsPipeline.h"
        "src/DX12/DX12GraphicsPipeline.cpp"
        "src/DX12/DX12ShaderTable.h"
        "src/DX12/DX12ShaderTable.cpp"
    )
    target_sources(${TARGET} PRIVATE ${VEX_DX12_SOURCES})

    # Link DX12 libraries
    target_link_libraries(${TARGET} PUBLIC d3d12.lib dxgi.lib dxguid.lib)

    # DirectX Agility SDK headers
    add_header_only_dependency(${TARGET} DirectXAgilitySDK "${DX_AGILITY_SDK_SOURCE_DIR}" "include" "directx")

    target_compile_definitions(${TARGET} PRIVATE 
        DIRECTX_AGILITY_SDK_VERSION=${DX_AGILITY_VERSION}
        D3D12_AGILITY_SDK_ENABLED
    )

    # Register PIX runtime DLL
    vex_add_files_to_target_property(${TARGET} "VEX_RUNTIME_DLLS" "${PIX_RUNTIME_DLL}")
    
    # Register D3D12 Agility SDK DLLs (these need special D3D12/ subdirectory)
    vex_add_files_to_target_property(${TARGET} "VEX_D3D12_AGILITY_DLLS" ${DX_AGILITY_RUNTIME_DLLS})

    message(STATUS "DirectX 12 backend configured successfully")
endfunction()

function(vex_setup_pix_events TARGET)
    if (VEX_ENABLE_DX12)
        set(PIX_EVENTS_DIR "${FETCHCONTENT_BASE_DIR}/PixEvents")

        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PIX_EVENTS_DIR}/bin/x64/WinPixEventRuntime.dll"
            "$<TARGET_FILE_DIR:${TARGET}>/WinPixEventRuntime.dll"
            COMMENT "Copying PixEvents DLLs to output directory : $<TARGET_FILE_DIR:${TARGET}>..."
        )
    endif()
endfunction()

function(vex_setup_d3d12_agility_runtime TARGET)
    if (VEX_ENABLE_DX12)
        set(VEX_D3D12_AGILITY_SOURCE_SDK_DIR "${DX_AGILITY_SDK_SOURCE_DIR}/bin/x64")

        if(NOT EXISTS "${VEX_D3D12_AGILITY_SOURCE_SDK_DIR}/D3D12Core.dll")
            message(STATUS ERROR " Missing D3D12Core.dll in Agility SDK directory: ${VEX_D3D12_AGILITY_SOURCE_SDK_DIR}.")
            return()
        endif()

        target_compile_definitions(Vex PRIVATE 
            DIRECTX_AGILITY_SDK_VERSION=${DX_AGILITY_SDK_VERSION}
            D3D12_AGILITY_SDK_ENABLED
        )

        target_compile_definitions(${TARGET} PRIVATE 
            DIRECTX_AGILITY_SDK_VERSION=${DX_AGILITY_SDK_VERSION}
            D3D12_AGILITY_SDK_ENABLED
        )

        target_sources(${TARGET} PRIVATE
            "${VEX_ROOT_DIR}/src/DX12/DX12AgilitySDK.cpp"
        )

        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET}>/D3D12"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VEX_D3D12_AGILITY_SOURCE_SDK_DIR}/D3D12Core.dll"
                "${VEX_D3D12_AGILITY_SOURCE_SDK_DIR}/d3d12SDKLayers.dll"
                "$<TARGET_FILE_DIR:${TARGET}>/D3D12"
            COMMENT "Copying Agility SDK's DLLs to output directory : $<TARGET_FILE_DIR:${TARGET}>/D3D12..."
        )

        message(STATUS "D3D12 Agility SDK 1.${DX_AGILITY_SDK_VERSION}.0 will be deployed with ${TARGET}")
    endif()
endfunction()
