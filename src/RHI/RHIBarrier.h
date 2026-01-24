#pragma once

#include <Vex/Texture.h>
#include <Vex/Types.h>
#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIFwd.h>

namespace vex
{

// Maps to VkPipelineStageFlags and D3D12_BARRIER_SYNC
enum class RHIBarrierSync : u8
{
    None,
    VertexInput,
    VertexShader,
    TessellationControl,
    TessellationEvaluation,
    GeometryShader,
    PixelShader,
    EarlyFragment,
    LateFragment,
    DepthStencil,
    ComputeShader,
    Copy,
    RenderTarget,
    DrawIndirect,
    // Synchronize for raytracing GPU execution.
    RayTracing,
    // Synchronize for building an acceleration structure.
    BuildAccelerationStructure,
    AllGraphics,
    AllCommands,
    // Supported only in Vulkan, is mapped to Copy in DX12.
    Blit,
    // Supported only in Vulkan, is mapped to All in DX12
    Clear,
};

// Maps to VkAccessFlags and D3D12_BARRIER_ACCESS
enum class RHIBarrierAccess : u8
{
    NoAccess,
    IndirectCommandRead,
    VertexInputRead,
    UniformRead,
    ShaderRead,
    ShaderReadWrite,
    RenderTarget,
    DepthStencilRead,
    DepthStencilWrite,
    DepthStencilReadWrite,
    CopySource,
    CopyDest,
    AccelerationStructureRead,
    AccelerationStructureWrite,
};

// Maps to VkImageLayout and D3D12_BARRIER_LAYOUT
enum class RHITextureLayout : u8
{
    Undefined,
    Common,            // General purpose (maps to VK_GENERAL, D3D12_COMMON)
    RenderTarget,      // Color render target
    DepthStencilRead,  // Depth/stencil read-only
    DepthStencilWrite, // Depth/stencil for writing
    ShaderResource,    // Shader read access
    UnorderedAccess,   // Storage/UAV access
    CopySource,        // Copy source
    CopyDest,          // Copy destination
    Present,           // Presentation
};

struct RHIBufferBarrier
{
    RHIBufferBarrier(NonNullPtr<RHIBuffer> buffer, RHIBarrierSync dstSync, RHIBarrierAccess dstAccess);

    NonNullPtr<RHIBuffer> buffer;

    RHIBarrierSync dstSync;
    RHIBarrierAccess dstAccess;
};

struct RHITextureBarrier
{
    RHITextureBarrier(NonNullPtr<RHITexture> texture,
                      TextureSubresource subresource,
                      RHIBarrierSync dstSync,
                      RHIBarrierAccess dstAccess,
                      RHITextureLayout dstLayout);

    NonNullPtr<RHITexture> texture;

    // Allows for applying a barrier to a specific texture subresource.
    // By default the barrier will be applied to the entire resource.
    TextureSubresource subresource;

    RHIBarrierSync dstSync;
    RHIBarrierAccess dstAccess;
    RHITextureLayout dstLayout;
};

} // namespace vex