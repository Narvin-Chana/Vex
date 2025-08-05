#include "DX12RHI.h"

#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>

#include <DX12/DX12Debug.h>
#include <DX12/DX12PhysicalDevice.h>
#include <DX12/DXGIFactory.h>
#include <DX12/HRChecker.h>
#include <DX12/RHI/DX12Buffer.h>
#include <DX12/RHI/DX12CommandList.h>
#include <DX12/RHI/DX12CommandPool.h>
#include <DX12/RHI/DX12DescriptorPool.h>
#include <DX12/RHI/DX12Fence.h>
#include <DX12/RHI/DX12PipelineState.h>
#include <DX12/RHI/DX12ResourceLayout.h>
#include <DX12/RHI/DX12SwapChain.h>
#include <DX12/RHI/DX12Texture.h>

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
    CleanupDebugMessageCallback(device);
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
    device = DXGIFactory::CreateDeviceStrict(
        static_cast<DX12PhysicalDevice*>(physicalDevice.get())->adapter.Get(),
        DX12FeatureChecker::ConvertFeatureLevelToDX12FeatureLevel(physicalDevice->featureChecker->GetFeatureLevel()));
    VEX_ASSERT(device.Get(), "D3D12 device creation must succeed");

    if (enableGPUDebugLayer)
    {
        SetupDebugMessageCallback(device);
        liveObjectsReporter.emplace();
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc{ .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       .Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
                                       .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                       .NodeMask = 0 };
        chk << device->CreateCommandQueue(&desc, IID_PPV_ARGS(&GetNativeQueue(CommandQueueType::Graphics)));
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc{ .Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                       .Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
                                       .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                       .NodeMask = 0 };
        chk << device->CreateCommandQueue(&desc, IID_PPV_ARGS(&GetNativeQueue(CommandQueueType::Compute)));
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc{ .Type = D3D12_COMMAND_LIST_TYPE_COPY,
                                       .Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
                                       .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                       .NodeMask = 0 };
        chk << device->CreateCommandQueue(&desc, IID_PPV_ARGS(&GetNativeQueue(CommandQueueType::Copy)));
    }
}

UniqueHandle<RHISwapChain> DX12RHI::CreateSwapChain(const SwapChainDescription& description,
                                                    const PlatformWindow& platformWindow)
{
    return MakeUnique<DX12SwapChain>(device, description, GetNativeQueue(CommandQueueType::Graphics), platformWindow);
}

UniqueHandle<RHICommandPool> DX12RHI::CreateCommandPool()
{
    return MakeUnique<DX12CommandPool>(device);
}

UniqueHandle<RHIGraphicsPipelineState> DX12RHI::CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key)
{
    GraphicsPipelineStateKey keyCopy = key;
    // Will clear out unsupported fields/validate that the user is not expecting invalid features.
    DX12GraphicsPipelineState::ClearUnsupportedKeyFields(keyCopy);
    return MakeUnique<DX12GraphicsPipelineState>(device, std::move(keyCopy));
}

UniqueHandle<RHIComputePipelineState> DX12RHI::CreateComputePipelineState(const ComputePipelineStateKey& key)
{
    return MakeUnique<DX12ComputePipelineState>(device, key);
}

UniqueHandle<RHIResourceLayout> DX12RHI::CreateResourceLayout(RHIDescriptorPool& descriptorPool)
{
    return MakeUnique<DX12ResourceLayout>(device);
}

UniqueHandle<RHITexture> DX12RHI::CreateTexture(const TextureDescription& description)
{
    return MakeUnique<DX12Texture>(device, description);
}

UniqueHandle<RHIBuffer> DX12RHI::CreateBuffer(const BufferDescription& description)
{
    return MakeUnique<DX12Buffer>(device, description);
}

UniqueHandle<RHIDescriptorPool> DX12RHI::CreateDescriptorPool()
{
    return MakeUnique<DX12DescriptorPool>(device);
}

void DX12RHI::ExecuteCommandList(RHICommandList& commandList)
{
    ID3D12CommandList* p = commandList.GetNativeCommandList().Get();
    queues[commandList.GetType()]->ExecuteCommandLists(1, &p);
}

void DX12RHI::ExecuteCommandLists(std::span<RHICommandList*> commandLists)
{
    std::array<std::vector<ID3D12CommandList*>, CommandQueueTypes::Count> rawCommandListsPerQueue;
    for (RHICommandList* cmdList : commandLists)
    {
        rawCommandListsPerQueue[cmdList->GetType()].push_back(cmdList->GetNativeCommandList().Get());
    }

    for (u32 i = 0; i < CommandQueueTypes::Count; ++i)
    {
        const auto& rawCmdLists = rawCommandListsPerQueue[i];
        if (rawCmdLists.empty())
        {
            continue;
        }
        queues[i]->ExecuteCommandLists(rawCmdLists.size(), rawCmdLists.data());
    }
}

UniqueHandle<RHIFence> DX12RHI::CreateFence(u32 numFenceIndices)
{
    return MakeUnique<DX12Fence>(numFenceIndices, device);
}

void DX12RHI::SignalFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex)
{
    chk << GetNativeQueue(queueType)->Signal(fence.fence.Get(), fence.GetFenceValue(fenceIndex));
}

void DX12RHI::WaitFence(CommandQueueType queueType, RHIFence& fence, u32 fenceIndex)
{
    chk << GetNativeQueue(queueType)->Wait(fence.fence.Get(), fence.GetFenceValue(fenceIndex));
}

void DX12RHI::ModifyShaderCompilerEnvironment(std::vector<const wchar_t*>& args, std::vector<ShaderDefine>& defines)
{
    args.push_back(L"-Qstrip_reflect");
    defines.emplace_back(L"VEX_DX12");
}

ComPtr<DX12Device>& DX12RHI::GetNativeDevice()
{
    return device;
}

ComPtr<ID3D12CommandQueue>& DX12RHI::GetNativeQueue(CommandQueueType queueType)
{
    return queues[std::to_underlying(queueType)];
}

DX12RHI::LiveObjectsReporter::~LiveObjectsReporter()
{
    // Output all live (potentially leaked) objects to the debug console
    ComPtr<IDXGIDebug1> dxgiDebug = nullptr;
    chk << DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));
    dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL,
                                 DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
}

} // namespace vex::dx12