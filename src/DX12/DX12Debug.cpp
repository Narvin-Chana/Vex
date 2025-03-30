#include "DX12Debug.h"

#include <string_view>

#include <Vex/Logger.h>

#include <DX12/DX12Headers.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

DWORD GDebugMessageCallbackCookie = 0;

void CALLBACK DebugMessageCallback(D3D12_MESSAGE_CATEGORY Category,
                                   D3D12_MESSAGE_SEVERITY Severity,
                                   D3D12_MESSAGE_ID ID,
                                   LPCSTR pDescription,
                                   void* pContext)
{
    LogLevel level;
    switch (Severity)
    {
    case D3D12_MESSAGE_SEVERITY_CORRUPTION:
    case D3D12_MESSAGE_SEVERITY_ERROR:
        level = Error;
        break;
    case D3D12_MESSAGE_SEVERITY_WARNING:
        level = Warning;
        break;
    case D3D12_MESSAGE_SEVERITY_INFO:
        level = Info;
        break;
    case D3D12_MESSAGE_SEVERITY_MESSAGE:
    default:
        level = Info;
        break;
    }

    std::string categoryStr;
    switch (Category)
    {
    case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED:
        categoryStr = "APPLICATION";
        break;
    case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS:
        categoryStr = "MISCELLANEOUS";
        break;
    case D3D12_MESSAGE_CATEGORY_INITIALIZATION:
        categoryStr = "INITIALIZATION";
        break;
    case D3D12_MESSAGE_CATEGORY_CLEANUP:
        categoryStr = "CLEANUP";
        break;
    case D3D12_MESSAGE_CATEGORY_COMPILATION:
        categoryStr = "COMPILATION";
        break;
    case D3D12_MESSAGE_CATEGORY_STATE_CREATION:
        categoryStr = "STATE_CREATION";
        break;
    case D3D12_MESSAGE_CATEGORY_STATE_SETTING:
        categoryStr = "STATE_SETTING";
        break;
    case D3D12_MESSAGE_CATEGORY_STATE_GETTING:
        categoryStr = "STATE_GETTING";
        break;
    case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION:
        categoryStr = "RESOURCE_MANIPULATION";
        break;
    case D3D12_MESSAGE_CATEGORY_EXECUTION:
        categoryStr = "EXECUTION";
        break;
    case D3D12_MESSAGE_CATEGORY_SHADER:
        categoryStr = "SHADER";
        break;
    default:
        categoryStr = "UNKNOWN";
        break;
    }

    VEX_LOG(level, "[DX12][{}][ID:{}] {}", categoryStr, std::to_string(ID), std::string_view(pDescription));
}

void SetupDebugMessageCallback(const ComPtr<DX12Device>& device)
{
    // Get the info queue from the device
    ComPtr<ID3D12InfoQueue1> infoQueue;
    chk << device->QueryInterface(IID_PPV_ARGS(&infoQueue));

    // Set the filter to break on corruption/error
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

    // Filter out noise messages
    D3D12_INFO_QUEUE_FILTER filter = {};
    D3D12_MESSAGE_ID denyIds[] = { D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                                   D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                                   D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE };

    filter.DenyList.NumIDs = _countof(denyIds);
    filter.DenyList.pIDList = denyIds;
    infoQueue->AddStorageFilterEntries(&filter);

    // Register the callback
    chk << infoQueue->RegisterMessageCallback(DebugMessageCallback,
                                              D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                                              nullptr,
                                              &GDebugMessageCallbackCookie);

    VEX_LOG(Info, "DX12 Debug callback registered successfully.");
}

void CleanupDebugMessageCallback(const ComPtr<DX12Device>& device)
{
    if (GDebugMessageCallbackCookie != 0)
    {
        // Get the info queue from the device
        ComPtr<ID3D12InfoQueue1> infoQueue;
        chk << device->QueryInterface(IID_PPV_ARGS(&infoQueue));

        // Register the callback
        chk << infoQueue->UnregisterMessageCallback(GDebugMessageCallbackCookie);
        GDebugMessageCallbackCookie = 0;

        VEX_LOG(Info, "DX12 Debug callback unregistered successfully.");
    }
}

void InitializeDebugLayer(bool enableGPUDebugLayer, bool enableGPUBasedValidation)
{
    ComPtr<ID3D12Debug6> debugInterface;
    chk << D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));

    if (enableGPUDebugLayer)
    {
        debugInterface->EnableDebugLayer();
    }

    debugInterface->SetEnableGPUBasedValidation(enableGPUBasedValidation);
    debugInterface->SetEnableSynchronizedCommandQueueValidation(enableGPUBasedValidation);
}

} // namespace vex::dx12