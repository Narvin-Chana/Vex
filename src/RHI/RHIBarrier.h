#pragma once

#include <Vex/Logger.h>
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
    // All "read" states.
    MemoryRead,
    // All "write" states.
    MemoryWrite,
};

// Maps to VkImageLayout and D3D12_BARRIER_LAYOUT
enum class RHITextureLayout : u8
{
    Undefined,
    Common,             // General purpose (maps to VK_GENERAL, D3D12_COMMON)
    RenderTarget,       // Color render target
    DepthStencilRead,   // Depth/stencil read-only
    DepthStencilWrite,  // Depth/stencil for writing
    ShaderRead,         // Shader read access
    ShaderReadWrite,    // Storage/UAV access
    CopySource,         // Copy source
    CopyDest,           // Copy destination
    Present,            // Presentation
};

struct RHIGlobalBarrier
{
    RHIBarrierSync srcSync;
    RHIBarrierSync dstSync;

    RHIBarrierAccess srcAccess;
    RHIBarrierAccess dstAccess;
};

struct RHITextureBarrier
{
    NonNullPtr<RHITexture> texture;
    TextureSubresource subresource;

    RHIBarrierSync srcSync;
    RHIBarrierSync dstSync;

    RHIBarrierAccess srcAccess;
    RHIBarrierAccess dstAccess;

    RHITextureLayout srcLayout;
    RHITextureLayout dstLayout;
};

struct RHIBufferBarrier
{
    NonNullPtr<RHIBuffer> buffer;

    RHIBarrierSync srcSync;
    RHIBarrierSync dstSync;

    RHIBarrierAccess srcAccess;
    RHIBarrierAccess dstAccess;
};

// Layout can be interpreted from certain access values.
inline RHITextureLayout RHIAccessToRHILayout(RHIBarrierAccess access)
{
    switch (access)
    {
    case RHIBarrierAccess::ShaderRead:
        return RHITextureLayout::ShaderRead;
    case RHIBarrierAccess::ShaderReadWrite:
        return RHITextureLayout::ShaderReadWrite;
    case RHIBarrierAccess::RenderTarget:
        return RHITextureLayout::RenderTarget;
    case RHIBarrierAccess::DepthStencilRead:
        return RHITextureLayout::DepthStencilRead;
    case RHIBarrierAccess::DepthStencilWrite:
    case RHIBarrierAccess::DepthStencilReadWrite:
        return RHITextureLayout::DepthStencilWrite;
    case RHIBarrierAccess::CopySource:
        return RHITextureLayout::CopySource;
    case RHIBarrierAccess::CopyDest:
        return RHITextureLayout::CopyDest;
    default:
        VEX_LOG(Fatal, "Cannot deduce the texture layout from your provided access: {}", access);
        std::unreachable();
    }
}

inline bool IsWriteAccess(RHIBarrierAccess access)
{
    using enum RHIBarrierAccess;
    switch (access)
    {
    case ShaderReadWrite:
    case RenderTarget:
    case DepthStencilWrite:
    case DepthStencilReadWrite:
    case CopyDest:
    case AccelerationStructureWrite:
    case MemoryWrite:
        return true;
    default:
        return false;
    }
};

} // namespace vex