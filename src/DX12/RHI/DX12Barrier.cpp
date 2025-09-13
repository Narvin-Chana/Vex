#include "DX12Barrier.h"

#include <utility>

#include <Vex/Logger.h>

namespace vex::dx12
{

D3D12_BARRIER_SYNC RHIBarrierSyncToDX12(RHIBarrierSync barrierSync)
{
    using enum RHIBarrierSync;

    switch (barrierSync)
    {
    case None:
        return D3D12_BARRIER_SYNC_NONE;
    case VertexInput:
        return D3D12_BARRIER_SYNC_INDEX_INPUT;
        return D3D12_BARRIER_SYNC_VERTEX_SHADING;
    case VertexShader:
    case TessellationControl:
    case TessellationEvaluation:
        // D3D12 doesn't have separate tessellation stages
    case GeometryShader:
        // Geometry shader is part of vertex shading in D3D12
        return D3D12_BARRIER_SYNC_VERTEX_SHADING;
    case PixelShader:
        return D3D12_BARRIER_SYNC_PIXEL_SHADING;
    case EarlyFragment:
    case LateFragment:
        // Early/Late Z is part of depth/stencil
    case Depth:
    case DepthStencil:
        return D3D12_BARRIER_SYNC_DEPTH_STENCIL;
    case ComputeShader:
        return D3D12_BARRIER_SYNC_COMPUTE_SHADING;
    case Copy:
        return D3D12_BARRIER_SYNC_COPY;
    // To be checked:
    // case Clear:
    //     return D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;
    case RenderTarget:
        return D3D12_BARRIER_SYNC_RENDER_TARGET;
    case DrawIndirect:
        return D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
    case Host:
        return D3D12_BARRIER_SYNC_NONE; // Host operations don't sync with GPU in D3D12
    case AllGraphics:
        return D3D12_BARRIER_SYNC_ALL_SHADING;
    case AllCommands:
        return D3D12_BARRIER_SYNC_ALL;
    default:
        VEX_LOG(Fatal, "Unsupported RHIBarrierSync!");
        std::unreachable();
    }
}

D3D12_BARRIER_ACCESS RHIBarrierAccessToDX12(RHIBarrierAccess barrierAccess)
{
    using enum RHIBarrierAccess;

    switch (barrierAccess)
    {
    case NoAccess:
        return D3D12_BARRIER_ACCESS_NO_ACCESS;
    case IndirectCommandRead:
        return D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
    case VertexInputRead:
        return D3D12_BARRIER_ACCESS_VERTEX_BUFFER | D3D12_BARRIER_ACCESS_INDEX_BUFFER;
    case UniformRead:
        return D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
    case ShaderRead:
        return D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
    case ShaderReadWrite:
        return D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
    case RenderTarget:
    case RenderTargetRead:
    case RenderTargetWrite:
        // D3D12 doesn't distinguish between RT read/write.
        return D3D12_BARRIER_ACCESS_RENDER_TARGET;
    case DepthStencil:
        return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
    case DepthStencilRead:
        return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
    case DepthStencilWrite:
        return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
    case CopySource:
        return D3D12_BARRIER_ACCESS_COPY_SOURCE;
    case CopyDest:
        return D3D12_BARRIER_ACCESS_COPY_DEST;
    case HostRead:
    case HostWrite:
        // Host access doesn't have direct D3D12 equivalent
        return D3D12_BARRIER_ACCESS_NO_ACCESS;
    case MemoryRead:
    case MemoryWrite:
        // Generic memory read/write
        return D3D12_BARRIER_ACCESS_COMMON;
    default:
        VEX_LOG(Fatal, "Unsupported RHIBarrierAccess type.");
        std::unreachable();
    }
}

D3D12_BARRIER_LAYOUT RHITextureLayoutToDX12(RHITextureLayout textureLayout)
{
    using enum RHITextureLayout;

    switch (textureLayout)
    {
    case Undefined:
        return D3D12_BARRIER_LAYOUT_UNDEFINED;
    case Common:
        return D3D12_BARRIER_LAYOUT_COMMON;
    case RenderTarget:
        return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
    case DepthStencilWrite:
        return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
    case DepthStencilRead:
        return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
    case ShaderResource:
        return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
    case UnorderedAccess:
        return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
    case CopySource:
        return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
    case CopyDest:
        return D3D12_BARRIER_LAYOUT_COPY_DEST;
    case Present:
        return D3D12_BARRIER_LAYOUT_PRESENT;
    default:
        VEX_LOG(Fatal, "Unsupported RHIBarrierAccess type.");
        std::unreachable();
    }
}

} // namespace vex::dx12