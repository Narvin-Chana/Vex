#include "DX12States.h"

namespace vex::dx12
{

D3D12_RESOURCE_STATES RHITextureStateToDX12State(RHITextureState state)
{
    switch (state)
    {
    case RHITextureState::RenderTarget:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case RHITextureState::ShaderReadWrite:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case RHITextureState::DepthWrite:
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    case RHITextureState::DepthRead:
        return D3D12_RESOURCE_STATE_DEPTH_READ;
    case RHITextureState::ShaderResource:
        return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
    case RHITextureState::CopySource:
        return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case RHITextureState::CopyDest:
        return D3D12_RESOURCE_STATE_COPY_DEST;
    case RHITextureState::Present:
    case RHITextureState::Common:
    default:
        return D3D12_RESOURCE_STATE_COMMON;
    }
}

D3D12_RESOURCE_STATES RHIBufferStateToDX12State(RHIBufferState::Flags state)
{
    // Handle mutually exclusive states first (highest priority)
    if (state & RHIBufferState::ShaderReadWrite)
    {
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    if (state & RHIBufferState::CopyDest)
    {
        return D3D12_RESOURCE_STATE_COPY_DEST;
    }
    if (state & RHIBufferState::CopySource)
    {
        return D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (state & RHIBufferState::IndirectArgs)
    {
        return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    }
    if (state & RHIBufferState::RaytracingAccelerationStructure)
    {
        return D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    }
    if (state & RHIBufferState::IndexBuffer)
    {
        return D3D12_RESOURCE_STATE_INDEX_BUFFER;
    }

    // These could be combined
    D3D12_RESOURCE_STATES combinedState = D3D12_RESOURCE_STATE_COMMON;
    if (state & (RHIBufferState::UniformResource | RHIBufferState::VertexBuffer))
    {
        combinedState |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    }
    if (state & RHIBufferState::ShaderResource)
    {
        combinedState |= D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
    }

    return (combinedState == D3D12_RESOURCE_STATE_COMMON) ? D3D12_RESOURCE_STATE_COMMON : combinedState;
}

} // namespace vex::dx12
