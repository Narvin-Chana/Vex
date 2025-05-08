#include "DX12RHI.h"

#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>

#include <DX12/DX12CommandPool.h>
#include <DX12/DX12Debug.h>
#include <DX12/DX12Fence.h>
#include <DX12/DX12PhysicalDevice.h>
#include <DX12/DX12PipelineState.h>
#include <DX12/DX12ResourceLayout.h>
#include <DX12/DX12Shader.h>
#include <DX12/DX12SwapChain.h>
#include <DX12/DX12Texture.h>
#include <DX12/DXGIFactory.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12RHI::DX12RHI(const PlatformWindowHandle& windowHandle, bool enableGPUDebugLayer, bool enableGPUBasedValidation)
    : enableGPUDebugLayer(enableGPUDebugLayer)
{
    HMODULE d3d12Module = GetModuleHandleA("D3D12Core.dll");
    if (d3d12Module)
    {
        char path[MAX_PATH];
        GetModuleFileNameA(d3d12Module, path, MAX_PATH);
        VEX_LOG(Info,
                "Using D3D12-SDK: {0}\n\tIf this path is in the project's target directory (and not in SYSTEM32), you "
                "are correctly using the D3D12-Agility-SDK!",
                std::string(path));
    }

    DXGIFactory::InitializeDXGIFactory();

    InitializeDebugLayer(enableGPUDebugLayer, enableGPUBasedValidation);
}

DX12RHI::~DX12RHI()
{
    if (enableGPUDebugLayer)
    {
        // Output all live (potentially leaked) objects to the debug console
        ComPtr<IDXGIDebug1> dxgiDebug = nullptr;
        chk << DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL,
                                     DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));

        CleanupDebugMessageCallback(device);
    }
}

std::vector<UniqueHandle<PhysicalDevice>> DX12RHI::EnumeratePhysicalDevices()
{
    std::vector<UniqueHandle<PhysicalDevice>> physicalDevices;

    u32 adapterIndex = 0;
    ComPtr<IDXGIAdapter4> adapter;

    while (DXGIFactory::dxgiFactory->EnumAdapterByGpuPreference(adapterIndex,
                                                                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                                IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND)
    {
        ComPtr<ID3D12Device> device;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device))) && device)
        {
            // Make sure we can cast the device to our chosen dx12 device type.
            ComPtr<DX12Device> minVersionDevice;
            device.As(&minVersionDevice);
            if (minVersionDevice)
            {
                physicalDevices.push_back(MakeUnique<DX12PhysicalDevice>(std::move(adapter), minVersionDevice));
                adapter = nullptr;
            }
        }

        adapterIndex++;
    }

    return physicalDevices;
}

void DX12RHI::Init(const UniqueHandle<PhysicalDevice>& physicalDevice)
{
    device = DXGIFactory::CreateDevice(
        static_cast<DX12PhysicalDevice*>(physicalDevice.get())->adapter.Get(),
        DX12FeatureChecker::ConvertFeatureLevelToDX12FeatureLevel(physicalDevice->featureChecker->GetFeatureLevel()));
    VEX_ASSERT(device.Get(), "D3D12 device creation must succeed");

    if (enableGPUDebugLayer)
    {
        SetupDebugMessageCallback(device);
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc{ .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       .Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
                                       .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                       .NodeMask = 0 };
        chk << device->CreateCommandQueue(&desc, IID_PPV_ARGS(&GetQueue(CommandQueueType::Graphics)));
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc{ .Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                       .Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
                                       .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                       .NodeMask = 0 };
        chk << device->CreateCommandQueue(&desc, IID_PPV_ARGS(&GetQueue(CommandQueueType::Compute)));
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc{ .Type = D3D12_COMMAND_LIST_TYPE_COPY,
                                       .Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
                                       .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                       .NodeMask = 0 };
        chk << device->CreateCommandQueue(&desc, IID_PPV_ARGS(&GetQueue(CommandQueueType::Copy)));
    }
}

UniqueHandle<RHISwapChain> DX12RHI::CreateSwapChain(const SwapChainDescription& description,
                                                    const PlatformWindow& platformWindow)
{
    return MakeUnique<DX12SwapChain>(device, description, GetQueue(CommandQueueType::Graphics), platformWindow);
}

UniqueHandle<RHICommandPool> DX12RHI::CreateCommandPool()
{
    return MakeUnique<DX12CommandPool>(device);
}

UniqueHandle<RHIShader> DX12RHI::CreateShader(const ShaderKey& key)
{
    return MakeUnique<DX12Shader>(key);
}

UniqueHandle<RHIGraphicsPipelineState> DX12RHI::CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key)
{
    return MakeUnique<DX12GraphicsPipelineState>(key);
}

UniqueHandle<RHIComputePipelineState> DX12RHI::CreateComputePipelineState(const ComputePipelineStateKey& key)
{
    return MakeUnique<DX12ComputePipelineState>(device, key);
}

UniqueHandle<RHIResourceLayout> DX12RHI::CreateResourceLayout(const FeatureChecker& featureChecker)
{
    return MakeUnique<DX12ResourceLayout>(device, reinterpret_cast<const DX12FeatureChecker&>(featureChecker));
}

UniqueHandle<RHITexture> DX12RHI::CreateTexture(const TextureDescription& description)
{
    return MakeUnique<DX12Texture>(device, description);
}

void DX12RHI::ExecuteCommandList(RHICommandList& commandList)
{
    ID3D12CommandList* p = static_cast<DX12CommandList&>(commandList).commandList.Get();
    queues[commandList.GetType()]->ExecuteCommandLists(1, &p);
}

UniqueHandle<RHIFence> DX12RHI::CreateFence(u32 numFenceIndices)
{
    return MakeUnique<DX12Fence>(numFenceIndices, device);
}

void DX12RHI::SignalFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex)
{
    chk << GetQueue(queueType)->Signal(static_cast<DX12Fence&>(fence).fence.Get(), fence.GetFenceValue(fenceIndex));
}

void DX12RHI::WaitFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex)
{
    chk << GetQueue(queueType)->Wait(static_cast<DX12Fence&>(fence).fence.Get(), fence.GetFenceValue(fenceIndex));
}

ComPtr<ID3D12CommandQueue>& DX12RHI::GetQueue(CommandQueueType queueType)
{
    return queues[std::to_underlying(queueType)];
}

} // namespace vex::dx12