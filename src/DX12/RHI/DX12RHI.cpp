#include "DX12RHI.h"

#include <Vex/Logger.h>
#include <Vex/PlatformWindow.h>
#include <Vex/RHIImpl/RHIAllocator.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHICommandPool.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>
#include <Vex/RHIImpl/RHIFence.h>
#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/RHIImpl/RHISwapChain.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Shaders/ShaderCompilerSettings.h>
#include <Vex/Shaders/ShaderEnvironment.h>
#include <Vex/Synchronization.h>

#include <DX12/DX12Debug.h>
#include <DX12/DX12PhysicalDevice.h>
#include <DX12/DXGIFactory.h>
#include <DX12/HRChecker.h>

namespace vex::dx12
{

namespace DX12RHI_Internal
{

static std::array<DX12Fence, CommandQueueTypes::Count> CreateFences(ComPtr<DX12Device>& device)
{
    return { DX12Fence(device), DX12Fence(device), DX12Fence(device) };
}

} // namespace DX12RHI_Internal

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
    // Must be done before removing the message callback.
    liveObjectsReporter.reset();

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
#if !VEX_SHIPPING
        GetNativeQueue(CommandQueueType::Graphics)->SetName(L"CommandQueue: Graphics");
#endif
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc{ .Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                       .Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
                                       .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                       .NodeMask = 0 };
        chk << device->CreateCommandQueue(&desc, IID_PPV_ARGS(&GetNativeQueue(CommandQueueType::Compute)));
#if !VEX_SHIPPING
        GetNativeQueue(CommandQueueType::Graphics)->SetName(L"CommandQueue: Compute");
#endif
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc{ .Type = D3D12_COMMAND_LIST_TYPE_COPY,
                                       .Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
                                       .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                       .NodeMask = 0 };
        chk << device->CreateCommandQueue(&desc, IID_PPV_ARGS(&GetNativeQueue(CommandQueueType::Copy)));
#if !VEX_SHIPPING
        GetNativeQueue(CommandQueueType::Graphics)->SetName(L"CommandQueue: Copy");
#endif
    }

    fences = DX12RHI_Internal::CreateFences(device);
}

RHISwapChain DX12RHI::CreateSwapChain(const SwapChainDescription& desc, const PlatformWindow& platformWindow)
{
    return DX12SwapChain(device, desc, GetNativeQueue(CommandQueueType::Graphics), platformWindow);
}

RHICommandPool DX12RHI::CreateCommandPool()
{
    return DX12CommandPool(*this, device);
}

RHIGraphicsPipelineState DX12RHI::CreateGraphicsPipelineState(const GraphicsPipelineStateKey& key)
{
    GraphicsPipelineStateKey keyCopy = key;
    // Will clear out unsupported fields/validate that the user is not expecting invalid features.
    DX12GraphicsPipelineState::ClearUnsupportedKeyFields(keyCopy);
    return { device, keyCopy };
}

RHIComputePipelineState DX12RHI::CreateComputePipelineState(const ComputePipelineStateKey& key)
{
    return { device, key };
}

RHIRayTracingPipelineState DX12RHI::CreateRayTracingPipelineState(const RayTracingPipelineStateKey& key)
{
    return { device, key };
}

RHIResourceLayout DX12RHI::CreateResourceLayout(RHIDescriptorPool& descriptorPool)
{
    return DX12ResourceLayout(device);
}

RHITexture DX12RHI::CreateTexture(RHIAllocator& allocator, const TextureDesc& desc)
{
    return DX12Texture(device, allocator, desc);
}

RHIBuffer DX12RHI::CreateBuffer(RHIAllocator& allocator, const BufferDesc& desc)
{
    return DX12Buffer(device, allocator, desc);
}

RHIDescriptorPool DX12RHI::CreateDescriptorPool()
{
    return DX12DescriptorPool(device);
}

RHIAllocator DX12RHI::CreateAllocator()
{
    return RHIAllocator(device);
}

void DX12RHI::ModifyShaderCompilerEnvironment(ShaderCompilerBackend compilerBackend, ShaderEnvironment& shaderEnv)
{
    if (compilerBackend == ShaderCompilerBackend::DXC)
    {
        shaderEnv.args.emplace_back(L"-Qstrip_reflect");
    }

    shaderEnv.defines.emplace_back("VEX_DX12");
}

void DX12RHI::WaitForTokenOnCPU(const SyncToken& syncToken)
{
    auto& fence = (*fences)[syncToken.queueType];
    fence.WaitOnCPU(syncToken.value);
}

bool DX12RHI::IsTokenComplete(const SyncToken& syncToken) const
{
    auto& fence = (*fences)[syncToken.queueType];
    return fence.GetValue() >= syncToken.value;
}

void DX12RHI::WaitForTokenOnGPU(CommandQueueType waitingQueue, const SyncToken& waitFor)
{
    auto& waitingFence = (*fences)[waitingQueue];
    auto& signalingFence = (*fences)[waitFor.queueType];

    // GPU-side wait: waitingQueue waits for waitFor.value on signalingQueue
    chk << GetNativeQueue(waitingQueue)->Wait(signalingFence.fence.Get(), waitFor.value);
}

std::array<SyncToken, CommandQueueTypes::Count> DX12RHI::GetMostRecentSyncTokenPerQueue() const
{
    std::array<SyncToken, CommandQueueTypes::Count> highestSyncTokens;

    for (u8 i = 0; i < CommandQueueTypes::Count; ++i)
    {
        highestSyncTokens[i] = { static_cast<CommandQueueType>(i),
                                 ((*fences)[i].nextSignalValue != 0) * ((*fences)[i].nextSignalValue - 1) };
    }

    return highestSyncTokens;
}

std::vector<SyncToken> DX12RHI::Submit(std::span<NonNullPtr<RHICommandList>> commandLists,
                                       std::span<SyncToken> dependencies)
{
    // Submit command lists
    std::array<std::vector<ID3D12CommandList*>, CommandQueueTypes::Count> rawCommandListsPerQueue;
    std::array<std::vector<NonNullPtr<RHICommandList>>, CommandQueueTypes::Count> commandListsPerQueue;
    for (NonNullPtr<RHICommandList> cmdList : commandLists)
    {
        rawCommandListsPerQueue[cmdList->GetType()].push_back(cmdList->GetNativeCommandList().Get());
        commandListsPerQueue[cmdList->GetType()].push_back(cmdList);
    }

    std::vector<SyncToken> syncTokens;
    syncTokens.reserve(CommandQueueTypes::Count);

    for (u32 i = 0; i < CommandQueueTypes::Count; ++i)
    {
        const auto& rawCmdLists = rawCommandListsPerQueue[i];
        if (rawCmdLists.empty())
        {
            continue;
        }
        queues[i]->ExecuteCommandLists(rawCmdLists.size(), rawCmdLists.data());

        // Signal N, and increment the queue fence.
        CommandQueueType queueType = static_cast<CommandQueueType>(i);
        auto& fence = (*fences)[queueType];
        u64 signalValue = fence.nextSignalValue++;
        chk << GetNativeQueue(queueType)->Signal(fence.fence.Get(), signalValue);

        syncTokens.push_back(SyncToken{ queueType, signalValue });
    }

    return syncTokens;
}

void DX12RHI::FlushGPU()
{
    // Wait for all queues to complete their latest work (the most recently signaled values)
    for (u32 i = 0; i < CommandQueueTypes::Count; ++i)
    {
        auto& fence = (*fences)[static_cast<CommandQueueType>(i)];
        fence.WaitOnCPU((fence.nextSignalValue != 0) * (fence.nextSignalValue - 1));
    }
}

ComPtr<DX12Device>& DX12RHI::GetNativeDevice()
{
    return device;
}

ComPtr<ID3D12CommandQueue>& DX12RHI::GetNativeQueue(CommandQueueType queueType)
{
    return queues[queueType];
}

DX12RHI::LiveObjectsReporter::~LiveObjectsReporter()
{
    // Output all live (potentially leaked) objects to the debug console
    ComPtr<IDXGIDebug1> dxgiDebug = nullptr;
    chk << DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));
    dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
}

} // namespace vex::dx12