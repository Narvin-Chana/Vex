#include "VkBarrier.h"

#include <utility>

#include <Vex/Logger.h>

namespace vex::vk
{

::vk::PipelineStageFlags2 RHIBarrierSyncToVulkan(RHIBarrierSync barrierSync)
{
    using enum RHIBarrierSync;
    using enum ::vk::PipelineStageFlagBits2;

    switch (barrierSync)
    {
    case None:
        return eNone;
    case VertexInput:
        return eVertexInput;
    case VertexShader:
        return eVertexShader;
    case TessellationControl:
        return eTessellationControlShader;
    case TessellationEvaluation:
        return eTessellationEvaluationShader;
    case GeometryShader:
        return eGeometryShader;
    case PixelShader:
        return eFragmentShader;
    case EarlyFragment:
        return eEarlyFragmentTests;
    case LateFragment:
        return eLateFragmentTests;
    case Depth:
    case DepthStencil:
        return eEarlyFragmentTests | eLateFragmentTests;
    case ComputeShader:
        return eComputeShader;
    case Copy:
        return eTransfer;
    case RenderTarget:
        return eColorAttachmentOutput;
    case DrawIndirect:
        return eDrawIndirect;
    case Host:
        return eHost;
    case AllGraphics:
        return eAllGraphics;
    case AllCommands:
        return eAllCommands;
    case Blit:
        return eBlit;
    case Clear:
        return eClear;
    default:
        VEX_LOG(Fatal, "Unsupported RHIBarrierSync!");
        std::unreachable();
    }
}

::vk::AccessFlags2 RHIBarrierAccessToVulkan(RHIBarrierAccess barrierAccess)
{
    using enum RHIBarrierAccess;
    using enum ::vk::AccessFlagBits2;

    switch (barrierAccess)
    {
    case NoAccess:
        return eNone;
    case IndirectCommandRead:
        return eIndirectCommandRead;
    case VertexInputRead:
        return eVertexAttributeRead | eIndexRead;
    case UniformRead:
        return eUniformRead;
    case ShaderRead:
        return eShaderRead;
    case ShaderReadWrite:
        return eShaderWrite;
    case RenderTarget:
        return eColorAttachmentRead | eColorAttachmentWrite;
    case RenderTargetRead:
        return eColorAttachmentRead;
    case RenderTargetWrite:
        return eColorAttachmentWrite;
    case DepthStencilRead:
        return eDepthStencilAttachmentRead;
    case DepthStencilWrite:
        return eDepthStencilAttachmentWrite;
    case DepthStencilReadWrite:
        return eDepthStencilAttachmentRead | eDepthStencilAttachmentWrite;
    case CopySource:
        return eTransferRead;
    case CopyDest:
        return eTransferWrite;
    case HostRead:
        return eHostRead;
    case HostWrite:
        return eHostWrite;
    case MemoryRead:
        return eMemoryRead;
    case MemoryWrite:
        return eMemoryWrite;
    default:
        VEX_LOG(Fatal, "Unsupported RHIBarrierAccess type.");
        std::unreachable();
    }
}

::vk::ImageLayout RHITextureLayoutToVulkan(RHITextureLayout textureLayout)
{
    using enum RHITextureLayout;
    using enum ::vk::ImageLayout;

    switch (textureLayout)
    {
    case Undefined:
        return eUndefined;
    case Common:
        return eGeneral;
    case RenderTarget:
        return eColorAttachmentOptimal;
    case DepthStencilRead:
        return eDepthStencilReadOnlyOptimal;
    case DepthStencilWrite:
        return eDepthStencilAttachmentOptimal;
    case ShaderResource:
        return eShaderReadOnlyOptimal;
    case UnorderedAccess:
        return eGeneral; // UAV/Storage requires general layout in Vulkan
    case CopySource:
        return eTransferSrcOptimal;
    case CopyDest:
        return eTransferDstOptimal;
    case Present:
        return ePresentSrcKHR;
    default:
        return eUndefined;
    }
}

} // namespace vex::vk