#include "DX12States.h"

namespace vex::dx12
{

D3D12_RESOURCE_STATES RHITextureStateToDX12State(RHITextureState::Flags state)
{
    D3D12_RESOURCE_STATES outState = D3D12_RESOURCE_STATE_COMMON;
    if (state & RHITextureState::RenderTarget)
    {
        outState |= D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    if (state & RHITextureState::UnorderedAccess)
    {
        outState |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    if (state & RHITextureState::DepthWrite)
    {
        outState |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    if (state & RHITextureState::DepthRead)
    {
        outState |= D3D12_RESOURCE_STATE_DEPTH_READ;
    }
    if (state & RHITextureState::ShaderResource)
    {
        outState |= D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
    }
    if (state & RHITextureState::CopySource)
    {
        outState |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (state & RHITextureState::CopyDest)
    {
        outState |= D3D12_RESOURCE_STATE_COPY_DEST;
    }
    // In DX12 PRESENT is the same state as COMMON, this means we have no need to check this state.

    return outState;
}

} // namespace vex::dx12
