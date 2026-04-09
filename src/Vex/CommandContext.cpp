#include "CommandContext.h"

#include <algorithm>
#include <array>
#include <functional>

#include <Vex/AccelerationStructure.h>
#include <Vex/BuiltInShaders/MipGeneration.h>
#include <Vex/DrawHelpers.h>
#include <Vex/Graphics.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/Platform/Debug.h>
#include <Vex/RHIImpl/RHIAccelerationStructure.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/RHIImpl/RHISwapChain.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/ResourceBindingUtils.h>
#include <Vex/Shaders/RayTracingShaders.h>
#include <Vex/Utility/ByteUtils.h>
#include <Vex/Utility/Validation.h>
#include <Vex/Utility/Visitor.h>

#include <RHI/RHIBarrier.h>
#include <RHI/RHIBindings.h>
#include <RHI/RHIPhysicalDevice.h>

namespace vex
{

namespace CommandContext_Internal
{

static std::vector<BufferTextureCopyDesc> GetBufferTextureCopyDescFromTextureRegions(const TextureDesc& desc,
                                                                                     Span<const TextureRegion> regions)
{
    // Otherwise we have to translate the TextureRegions to their equivalent BufferTextureCopyDescs.
    std::vector<BufferTextureCopyDesc> copyDescs;
    copyDescs.reserve(regions.size());

    const float bytesPerPixel = TextureUtil::GetPixelByteSizeFromFormat(desc.format);
    u64 stagingBufferOffset = 0;

    for (const TextureRegion& region : regions)
    {
        for (u16 mip = region.subresource.startMip;
             mip < region.subresource.startMip + region.subresource.GetMipCount(desc);
             ++mip)
        {
            for (u32 slice = region.subresource.startSlice;
                 slice < region.subresource.startSlice + region.subresource.GetSliceCount(desc);
                 slice++)
            {
                const u32 mipWidth = region.extent.GetWidth(desc, mip);
                const u32 mipHeight = region.extent.GetHeight(desc, mip);
                const u32 mipDepth = region.extent.GetDepth(desc, mip);

                // Calculate the size of this region in the staging buffer.
                const u32 alignedRowPitch = AlignUp<u32>(mipWidth * bytesPerPixel, TextureUtil::RowPitchAlignment);
                const u64 regionStagingSize = static_cast<u64>(alignedRowPitch) * mipHeight * mipDepth;

                BufferTextureCopyDesc copyDesc{
                    .bufferRegion = { .offset = stagingBufferOffset, .byteSize = regionStagingSize, },
                    .textureRegion = {
                        .subresource = {
                            .startMip = mip,
                            .mipCount = 1,
                            .startSlice = slice,
                            .sliceCount = 1,
                            .aspect = region.subresource.aspect
                        },
                        .offset = region.offset,
                        .extent = region.extent,
                    },
                };

                copyDescs.push_back(std::move(copyDesc));

                stagingBufferOffset += AlignUp<u64>(regionStagingSize, TextureUtil::SliceAlignment);
            }

            // Move to next aligned position in staging buffer.
            stagingBufferOffset = AlignUp<u64>(stagingBufferOffset, TextureUtil::MipAlignment);
        }
    }

    return copyDescs;
}

static GraphicsPipelineStateKey GetGraphicsPSOKeyFromDrawDesc(const DrawDesc& drawDesc,
                                                              const RHIDrawResources& rhiDrawRes)
{
    GraphicsPipelineStateKey key{ .vertexShader = drawDesc.vertexShader,
                                  .pixelShader = drawDesc.pixelShader,
                                  .vertexInputLayout = drawDesc.vertexInputLayout,
                                  .inputAssembly = drawDesc.inputAssembly,
                                  .rasterizerState = drawDesc.rasterizerState,
                                  .depthStencilState = drawDesc.depthStencilState,
                                  .colorBlendState = drawDesc.colorBlendState };

    for (const RHITextureBinding& rhiBinding : rhiDrawRes.renderTargets)
    {
        key.renderTargetState.colorFormats.emplace_back(rhiBinding.binding.texture.desc.format,
                                                        rhiBinding.binding.isSRGB);
    }

    if (rhiDrawRes.depthStencil)
    {
        key.renderTargetState.depthStencilFormat = rhiDrawRes.depthStencil->binding.texture.desc.format;
    }

    // Ensure each render target has atleast a default color attachment (no blending, write all).
    key.colorBlendState.attachments.resize(rhiDrawRes.renderTargets.size());

    return key;
}

} // namespace CommandContext_Internal

CommandContext::CommandContext(NonNullPtr<Graphics> graphics,
                               NonNullPtr<RHICommandList> cmdList,
                               NonNullPtr<RHITimestampQueryPool> queryPool)
    : graphics(graphics)
    , cmdList(cmdList)
{
    cmdList->Open();
    cmdList->SetTimestampQueryPool(queryPool);
    if (cmdList->GetQueue() != QueueType::Copy)
    {
        cmdList->SetDescriptorPool(*graphics->descriptorPool, graphics->psCache->GetResourceLayout());
    }
}

CommandContext::~CommandContext()
{
    // This must be disabled for tests, as it interferes with gtest's crash catching logic (this intercepts the actual
    // error message). This is due to the fact that objects inside the test are destroyed upon test cleanup.
#ifndef VEX_TESTS
    VEX_CHECK(!cmdList->IsOpen(),
              "A command context was destroyed while still being open for commands, remember to submit your command "
              "context to the GPU using vex::Graphics::Submit()!");
#endif
}

QueueType CommandContext::GetQueue() const
{
    return cmdList->GetQueue();
}

void CommandContext::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
    cmdList->SetViewport(x, y, width, height, minDepth, maxDepth);
    hasInitializedViewport = true;
}

void CommandContext::SetScissor(i32 x, i32 y, u32 width, u32 height)
{
    cmdList->SetScissor(x, y, width, height);
    hasInitializedScissor = true;
}

void CommandContext::ClearTexture(const Texture& texture,
                                  std::optional<TextureClearValue> textureClearValue,
                                  const TextureSubresource& subresource,
                                  Span<const TextureClearRect> clearRects)
{
    VEX_CHECK(
        texture.desc.usage & (TextureUsage::RenderTarget | TextureUsage::DepthStencil),
        "ClearUsage not supported on this texture, it must be either usable as a render target or as a depth stencil!");
    TextureUtil::ValidateSubresource(texture.desc, subresource);

    RHITexture& rhiTexture = graphics->GetRHITexture(texture.handle);

    const auto [dstSync, dstAccess, dstLayout] = cmdList->GetClearTextureBarrierState(texture.desc, clearRects);
    EnqueueTextureBarrier(texture, subresource, dstSync, dstAccess, dstLayout);
    FlushBarriers();

    TextureClearValue clearValue = textureClearValue.value_or(texture.desc.clearValue);
    cmdList->ClearTexture(
        rhiTexture,
        subresource,
        // This is a safe cast, textures can only contain one of the two usages (RT/DS).
        static_cast<TextureUsage::Type>(texture.desc.usage & (TextureUsage::RenderTarget | TextureUsage::DepthStencil)),
        std::move(clearValue),
        clearRects);
}

void CommandContext::Draw(const DrawDesc& drawDesc,
                          const DrawResourceBinding& drawBindings,
                          ConstantBinding constants,
                          Span<const ResourceBinding> trackedResources,
                          u32 vertexCount,
                          u32 instanceCount,
                          u32 vertexOffset,
                          u32 instanceOffset)
{
    CheckViewportAndScissor();

    // Index buffers are not used in Draw, warn the user if they have still bound one.
    if (drawBindings.indexBuffer.has_value())
    {
        VEX_LOG(Warning,
                "Your CommandContext::Draw call resources contain an index buffer which will be ignored. If you wish "
                "to use the index buffer, call CommandContext::DrawIndexed instead.");
    }

    auto drawResources = PrepareDrawCall(drawDesc, drawBindings, constants, trackedResources);
    FlushBarriers();
    if (!drawResources.has_value())
    {
        return;
    }

    cmdList->BeginRendering(*drawResources);
    // TODO(https://trello.com/c/IGxuLci9): Validate draw vertex count (eg: versus the currently used vertex buffer
    // size)
    cmdList->Draw(vertexCount, instanceCount, vertexOffset, instanceOffset);
    cmdList->EndRendering();
}

void CommandContext::DrawIndexed(const DrawDesc& drawDesc,
                                 const DrawResourceBinding& drawBindings,
                                 ConstantBinding constants,
                                 Span<const ResourceBinding> trackedResources,
                                 u32 indexCount,
                                 u32 instanceCount,
                                 u32 indexOffset,
                                 u32 vertexOffset,
                                 u32 instanceOffset)
{
    CheckViewportAndScissor();
    auto drawResources = PrepareDrawCall(drawDesc, drawBindings, constants, trackedResources);
    FlushBarriers();
    if (!drawResources.has_value())
    {
        return;
    }

    cmdList->BeginRendering(*drawResources);
    // TODO(https://trello.com/c/IGxuLci9): Validate draw index count (eg: versus the currently used index buffer size)
    cmdList->DrawIndexed(indexCount, instanceCount, indexOffset, vertexOffset, instanceOffset);
    cmdList->EndRendering();
}

void CommandContext::DrawIndirect()
{
    CheckViewportAndScissor();

    FlushBarriers();
    VEX_NOT_YET_IMPLEMENTED();
}

void CommandContext::DrawIndexedIndirect()
{
    CheckViewportAndScissor();

    FlushBarriers();
    VEX_NOT_YET_IMPLEMENTED();
}

void CommandContext::Dispatch(const ShaderKey& shader,
                              ConstantBinding constants,
                              Span<const ResourceBinding> trackedResources,
                              std::array<u32, 3> groupCount)
{
    using namespace CommandContext_Internal;

    InferResourceBarriers(RHIBarrierSync::ComputeShader, trackedResources);
    FlushBarriers();

    ComputePipelineStateKey psoKey = { .computeShader = shader };
    if (!cachedComputePSOKey || psoKey != cachedComputePSOKey)
    {
        std::unique_ptr<RHIComputePipelineState> oldPSO;

        // Register shader and get Pipeline if exists (if not create it).
        const RHIComputePipelineState* pipelineState = graphics->psCache->GetComputePipelineState(psoKey, oldPSO);
        if (oldPSO)
        {
            temporaryResources.emplace_back(std::move(oldPSO));
        }

        // Nothing more to do if the PSO is invalid.
        if (!pipelineState)
        {
            VEX_LOG(Error, "PSO cache returned an invalid pipeline state, unable to continue dispatch...");
            return;
        }
        cmdList->SetPipelineState(*pipelineState);
        cachedComputePSOKey = psoKey;
    }

    // Sets the resource layout to use for the dispatch.
    RHIResourceLayout& resourceLayout = graphics->psCache->GetResourceLayout();
    resourceLayout.SetLayoutResources(constants);
    cmdList->SetLayout(resourceLayout);

    // Validate dispatch (vs platform/api constraints)
    // graphics->ValidateDispatch(groupCount);

    // Perform dispatch
    cmdList->Dispatch(groupCount);
}

void CommandContext::DispatchIndirect()
{
    VEX_NOT_YET_IMPLEMENTED();
}

void CommandContext::TraceRays(const RayTracingCollection& rayTracingCollection,
                               ConstantBinding constants,
                               Span<const ResourceBinding> trackedResources,
                               const TraceRaysDesc& rayTracingArgs)
{
    VEX_CHECK(GPhysicalDevice->IsFeatureSupported(Feature::RayTracing),
              "Your GPU does not support ray tracing, unable to dispatch rays!");
    RayTracingCollection::ValidateShaderTypes(rayTracingCollection);

    InferResourceBarriers(RHIBarrierSync::RayTracing, trackedResources);
    FlushBarriers();

    std::unique_ptr<RHIRayTracingPipelineState> oldPSO;
    std::vector<MaybeUninitialized<RHIBuffer>> oldSBTs;

    const RHIRayTracingPipelineState* pipelineState =
        graphics->psCache->GetRayTracingPipelineState(rayTracingCollection, *graphics->allocator, oldPSO, oldSBTs);
    if (oldPSO)
    {
        temporaryResources.emplace_back(std::move(oldPSO));
    }
    if (!oldSBTs.empty())
    {
        for (auto& resource : oldSBTs)
        {
            temporaryResources.emplace_back(std::move(resource));
        }
    }
    if (!pipelineState)
    {
        VEX_LOG(Error, "PSO cache returned an invalid pipeline state, unable to continue dispatch...");
        return;
    }
    cmdList->SetPipelineState(*pipelineState);

    // Sets the resource layout to use for the ray trace.
    RHIResourceLayout& resourceLayout = graphics->psCache->GetResourceLayout();
    resourceLayout.SetLayoutResources(constants);

    cmdList->SetLayout(resourceLayout);

    // Validate ray trace (vs platform/api constraints)
    // graphics->ValidateTraceRays(widthHeightDepth);

    cmdList->TraceRays(rayTracingArgs, *pipelineState);
}

void CommandContext::GenerateMips(const TextureBinding& textureBinding)
{
    const Texture& texture = textureBinding.texture;

    VEX_CHECK(textureBinding.subresource.startSlice == 0 && textureBinding.subresource.GetSliceCount(texture.desc),
              "Mip Generation must take into account all slices.");
    VEX_CHECK(texture.desc.mips > 1,
              "The texture must have more than atleast 1 mip in order to have the other mips generated.");
    VEX_CHECK(textureBinding.subresource.GetMipCount(texture.desc) >= 1, "You must generate at least one mip.");
    VEX_CHECK(textureBinding.subresource.startMip < texture.desc.mips,
              "The startMip index must be smaller than the last mip in order to have the other mips generated.");

    u16 sourceMip = textureBinding.subresource.startMip;
    u16 lastDestMip = sourceMip + textureBinding.subresource.GetMipCount(texture.desc) - 1;

    const bool apiFormatSupportsLinearFiltering =
        GPhysicalDevice->FormatSupportsLinearFiltering(texture.desc.format, textureBinding.isSRGB);
    const bool textureFormatSupportsMipGeneration = FormatUtil::SupportsMipGeneration(texture.desc.format);
    VEX_CHECK(textureFormatSupportsMipGeneration && apiFormatSupportsLinearFiltering,
              "The texture's format must be a valid format for mip generation. Only uncompressed floating point / "
              "normalized color formats are supported.");

    VEX_CHECK(cmdList->GetQueue() != QueueType::Copy,
              "Mip Generation requires a Compute or Graphics command list type.");

    // Built-in mip generation is leveraged if supported (and if we're using a graphics command queue).
    // If we want to perform SRGB mip generation, we must do it manually.
    if (GPhysicalDevice->HasCapability(Capability::MipGeneration) && cmdList->GetQueue() == QueueType::Graphics &&
        !textureBinding.isSRGB)
    {
        EnqueueTextureBarrier(texture,
                              { .startMip = sourceMip, .mipCount = 1 },
                              RHIBarrierSync::Blit,
                              RHIBarrierAccess::CopySource,
                              RHITextureLayout::CopySource);
        EnqueueTextureBarrier(texture,
                              { .startMip = static_cast<u16>(sourceMip + 1), .mipCount = GTextureAllMips },
                              RHIBarrierSync::Blit,
                              RHIBarrierAccess::CopyDest,
                              RHITextureLayout::CopyDest);
        FlushBarriers();
        cmdList->GenerateMips(graphics->GetRHITexture(texture.handle), textureBinding.subresource);
        return;
    }

    auto GetTextureDimensionDefine = [desc = &texture.desc](TextureType type) -> std::string
    {
        switch (type)
        {
        case TextureType::Texture2D:
            return (desc->GetSliceCount() > 1) ? "1" : "0"; // 2DArray or 2D
        case TextureType::TextureCube:
            return (desc->GetSliceCount() > 6) ? "3" : "2"; // CubeArray or Cube
        case TextureType::Texture3D:
            return "4";
        default:
            return "0";
        }
    };

    // We have to perform manual mip generation if not supported by the graphics API.
    ShaderKey shaderKey = MipGenerationShaderKey;
    shaderKey.defines = { ShaderDefine{ "TEXTURE_TYPE", std::string(FormatUtil::GetHLSLType(texture.desc.format)) },
                          ShaderDefine{ "TEXTURE_DIMENSION", GetTextureDimensionDefine(texture.desc.type) },
                          ShaderDefine{ "LINEAR_SAMPLER_SLOT", std::format("s{}", graphics->builtInLinearSamplerSlot) },
                          ShaderDefine{ "CONVERT_TO_SRGB", textureBinding.isSRGB ? "1" : "0" },
                          ShaderDefine{ "NON_POWER_OF_TWO" } };
    const u32 nonPowerOfTwoDefineIndex = shaderKey.defines.size() - 1;

    static auto ComputeNPOTFlag = [](u32 srcWidth, u32 srcHeight, u32 srcDepth, bool is3D) -> u32
    {
        if (!is3D)
        {
            // 2D logic
            bool xRatio = (srcWidth / std::max(1u, srcWidth >> 1)) > 2;
            bool yRatio = (srcHeight / std::max(1u, srcHeight >> 1)) > 2;
            return (xRatio ? 1 : 0) | (yRatio ? 2 : 0);
        }
        else
        {
            // 3D logic
            bool xRatio = (srcWidth / std::max(1u, srcWidth >> 1)) > 2;
            bool yRatio = (srcHeight / std::max(1u, srcHeight >> 1)) > 2;
            bool zRatio = (srcDepth / std::max(1u, srcDepth >> 1)) > 2;
            return (xRatio ? 1 : 0) | (yRatio ? 2 : 0) | (zRatio ? 4 : 0);
        }
    };

    struct Uniforms
    {
        std::array<float, 3> texelSize;
        BindlessHandle sourceMipHandle;
        u32 sourceMipLevel;
        u32 numMips;
        BindlessHandle destinationMip0;
        BindlessHandle destinationMip1;
    };

    EnqueueTextureBarrier(texture,
                          { .startMip = sourceMip, .mipCount = 1 },
                          RHIBarrierSync::ComputeShader,
                          RHIBarrierAccess::ShaderRead,
                          RHITextureLayout::ShaderRead);

    EnqueueTextureBarrier(texture,
                          { .startMip = static_cast<u16>(sourceMip + 1), .mipCount = GTextureAllMips },
                          RHIBarrierSync::ComputeShader,
                          RHIBarrierAccess::ShaderReadWrite,
                          RHITextureLayout::ShaderReadWrite);

    u32 width = texture.desc.width;
    u32 height = texture.desc.height;
    u32 depth = texture.desc.GetDepth();
    bool isLastIteration = false;

    for (u16 mip = sourceMip + 1; mip <= lastDestMip;)
    {
        if (mip >= lastDestMip)
        {
            isLastIteration = true;
        }

        shaderKey.defines[nonPowerOfTwoDefineIndex].value =
            std::to_string(ComputeNPOTFlag(width, height, depth, texture.desc.type == TextureType::Texture3D));

        std::vector<ResourceBinding> bindings{
            TextureBinding{
                .texture = texture,
                .usage = TextureBindingUsage::ShaderRead,
                .isSRGB = textureBinding.isSRGB,
                .subresource = { .startMip = static_cast<u16>(mip - 1u), .mipCount = 1 },
            },
            TextureBinding{
                .texture = texture,
                .usage = TextureBindingUsage::ShaderReadWrite,
                // Cannot have SRGB ShaderReadWrite, we manually perform color space conversion in the shader.
                .isSRGB = false,
                .subresource = { .startMip = mip, .mipCount = 1 },
            },
        };
        if (!isLastIteration)
        {
            bindings.push_back(TextureBinding{
                .texture = texture,
                .usage = TextureBindingUsage::ShaderReadWrite,
                .isSRGB = false,
                .subresource = { .startMip = static_cast<u16>(mip + 1u), .mipCount = 1 },
            });
        }
        auto handles = graphics->GetBindlessHandles(bindings);

        Uniforms uniforms{
            {
                2.0f / width,
                2.0f / height,
                2.0f / depth,
            },
            handles[0],
            mip - 1u,
            1u + !isLastIteration,
            handles[1],
            !isLastIteration ? handles[2] : BindlessHandle{},
        };

        // For 2D: z = 1
        // For 2DArray: z = number of slices
        // For Cube: z = 6 * faces
        // For CubeArray: z = 6 * faces * numCubes
        // For 3D: z = depth
        u32 dispatchZ = texture.desc.type == TextureType::Texture3D ? depth : texture.desc.GetSliceCount();
        std::array<u32, 3> dispatchGroupCount{ (width + 7u) / 8u, (height + 7u) / 8u, dispatchZ };
        Dispatch(shaderKey, ConstantBinding(uniforms), bindings, dispatchGroupCount);

        width = std::max(1u, width >> (1 + !isLastIteration));
        height = std::max(1u, height >> (1 + !isLastIteration));
        depth = std::max(1u, depth >> (1 + !isLastIteration));

        mip += 1 + !isLastIteration;
    }
}

void CommandContext::Copy(const Texture& source, const Texture& destination)
{
    VEX_CHECK(source.handle != destination.handle, "Cannot copy a texture entirely to itself!");

    TextureUtil::ValidateCompatibleTextureDescs(source.desc, destination.desc);

    EnqueueTextureBarrier(source,
                          TextureSubresource{},
                          RHIBarrierSync::Copy,
                          RHIBarrierAccess::CopySource,
                          RHITextureLayout::CopySource);
    EnqueueTextureBarrier(destination,
                          TextureSubresource{},
                          RHIBarrierSync::Copy,
                          RHIBarrierAccess::CopyDest,
                          RHITextureLayout::CopyDest);
    FlushBarriers();

    RHITexture& sourceRHI = graphics->GetRHITexture(source.handle);
    RHITexture& destinationRHI = graphics->GetRHITexture(destination.handle);
    cmdList->Copy(sourceRHI, destinationRHI);
}

void CommandContext::Copy(const Texture& source, const Texture& destination, const TextureCopyDesc& regionMapping)
{
    Copy(source, destination, { &regionMapping, 1 });
}

void CommandContext::Copy(const Texture& source, const Texture& destination, Span<const TextureCopyDesc> regionMappings)
{
    for (auto& mapping : regionMappings)
    {
        TextureUtil::ValidateCopyDesc(source.desc, destination.desc, mapping);
        EnqueueTextureBarrier(source,
                              mapping.srcRegion.subresource,
                              RHIBarrierSync::Copy,
                              RHIBarrierAccess::CopySource,
                              RHITextureLayout::CopySource);
        EnqueueTextureBarrier(destination,
                              mapping.dstRegion.subresource,
                              RHIBarrierSync::Copy,
                              RHIBarrierAccess::CopyDest,
                              RHITextureLayout::CopyDest);
    }
    FlushBarriers();

    RHITexture& sourceRHI = graphics->GetRHITexture(source.handle);
    RHITexture& destinationRHI = graphics->GetRHITexture(destination.handle);
    cmdList->Copy(sourceRHI, destinationRHI, regionMappings);
}

void CommandContext::Copy(const Buffer& source, const Buffer& destination)
{
    VEX_CHECK(source.handle != destination.handle, "Cannot copy a buffer entirely to itself!");

    BufferUtil::ValidateSimpleBufferCopy(source.desc, destination.desc);

    // Makes sure writes to the source are done.
    EnqueueGlobalBarrier({
        .srcSync = RHIBarrierSync::AllCommands,
        .dstSync = RHIBarrierSync::Copy,
        .srcAccess = RHIBarrierAccess::MemoryWrite,
        .dstAccess = RHIBarrierAccess::CopySource,
    });
    // Makes sure reads of the destination are done.
    EnqueueGlobalBarrier({
        .srcSync = RHIBarrierSync::AllCommands,
        .dstSync = RHIBarrierSync::Copy,
        .srcAccess = RHIBarrierAccess::MemoryRead,
        .dstAccess = RHIBarrierAccess::CopyDest,
    });
    FlushBarriers();

    RHIBuffer& sourceRHI = graphics->GetRHIBuffer(source.handle);
    RHIBuffer& destinationRHI = graphics->GetRHIBuffer(destination.handle);
    cmdList->Copy(sourceRHI, destinationRHI);
}

void CommandContext::Copy(const Buffer& source, const Buffer& destination, const BufferCopyDesc& bufferCopyDesc)
{
    BufferUtil::ValidateBufferCopyDesc(source.desc, destination.desc, bufferCopyDesc);

    // Makes sure writes to the source are done.
    EnqueueGlobalBarrier({
        .srcSync = RHIBarrierSync::AllCommands,
        .dstSync = RHIBarrierSync::Copy,
        .srcAccess = RHIBarrierAccess::MemoryWrite,
        .dstAccess = RHIBarrierAccess::CopySource,
    });
    // Makes sure reads of the destination are done.
    EnqueueGlobalBarrier({
        .srcSync = RHIBarrierSync::AllCommands,
        .dstSync = RHIBarrierSync::Copy,
        .srcAccess = RHIBarrierAccess::MemoryRead,
        .dstAccess = RHIBarrierAccess::CopyDest,
    });
    FlushBarriers();

    RHIBuffer& sourceRHI = graphics->GetRHIBuffer(source.handle);
    RHIBuffer& destinationRHI = graphics->GetRHIBuffer(destination.handle);
    cmdList->Copy(sourceRHI, destinationRHI, bufferCopyDesc);
}

void CommandContext::Copy(const Buffer& source, const Texture& destination)
{
    // Makes sure writes to the source buffer are done.
    EnqueueGlobalBarrier({
        .srcSync = RHIBarrierSync::AllCommands,
        .dstSync = RHIBarrierSync::Copy,
        .srcAccess = RHIBarrierAccess::MemoryWrite,
        .dstAccess = RHIBarrierAccess::CopySource,
    });
    // Makes sure that reads of the dst texture are finished.
    EnqueueTextureBarrier(destination,
                          TextureSubresource{},
                          RHIBarrierSync::Copy,
                          RHIBarrierAccess::CopyDest,
                          RHITextureLayout::CopyDest);
    FlushBarriers();

    RHIBuffer& sourceRHI = graphics->GetRHIBuffer(source.handle);
    RHITexture& destinationRHI = graphics->GetRHITexture(destination.handle);
    cmdList->Copy(sourceRHI, destinationRHI);
}

void CommandContext::Copy(const Buffer& source, const Texture& destination, const BufferTextureCopyDesc& copyDesc)
{
    Copy(source, destination, { &copyDesc, 1 });
}

void CommandContext::Copy(const Buffer& source, const Texture& destination, Span<const BufferTextureCopyDesc> copyDescs)
{
    for (auto& copyDesc : copyDescs)
    {
        TextureCopyUtil::ValidateBufferTextureCopyDesc(source.desc, destination.desc, copyDesc);
    }

    // Makes sure writes to the source buffer are done.
    EnqueueGlobalBarrier({
        .srcSync = RHIBarrierSync::AllCommands,
        .dstSync = RHIBarrierSync::Copy,
        .srcAccess = RHIBarrierAccess::MemoryWrite,
        .dstAccess = RHIBarrierAccess::CopySource,
    });
    for (const auto& cd : copyDescs)
    {
        // Makes sure that reads of the dest texture subresources are finished.
        EnqueueTextureBarrier(destination,
                              cd.textureRegion.subresource,
                              RHIBarrierSync::Copy,
                              RHIBarrierAccess::CopyDest,
                              RHITextureLayout::CopyDest);
    }
    FlushBarriers();

    RHIBuffer& sourceRHI = graphics->GetRHIBuffer(source.handle);
    RHITexture& destinationRHI = graphics->GetRHITexture(destination.handle);
    cmdList->Copy(sourceRHI, destinationRHI, copyDescs);
}

void CommandContext::Copy(const Texture& source, const Buffer& destination)
{
    Copy(source, destination, BufferTextureCopyDesc::AllMips(source.desc));
}

void CommandContext::Copy(const Texture& source,
                          const Buffer& destination,
                          Span<const BufferTextureCopyDesc> bufferToTextureCopyDescriptions)
{
    TextureAspect::Flags aspects = 0;
    for (const auto& copyDesc : bufferToTextureCopyDescriptions)
    {
        TextureCopyUtil::ValidateBufferTextureCopyDesc(destination.desc, source.desc, copyDesc);
        aspects |= copyDesc.textureRegion.subresource.GetAspect(source.desc);
    }

    for (const auto& copyDesc : bufferToTextureCopyDescriptions)
    {
        // Make sure that writes to the source texture regions are finished.
        TextureSubresource regionSubresource = copyDesc.textureRegion.subresource;
        regionSubresource.aspect = aspects;
        EnqueueTextureBarrier(source,
                              regionSubresource,
                              RHIBarrierSync::Copy,
                              RHIBarrierAccess::CopySource,
                              RHITextureLayout::CopySource);
    }

    // Make sure that reads of the dest buffer are finished.
    EnqueueGlobalBarrier({
        .srcSync = RHIBarrierSync::AllCommands,
        .dstSync = RHIBarrierSync::Copy,
        .srcAccess = RHIBarrierAccess::MemoryRead,
        .dstAccess = RHIBarrierAccess::CopyDest,
    });
    FlushBarriers();

    RHITexture& sourceRHI = graphics->GetRHITexture(source.handle);
    RHIBuffer& destinationRHI = graphics->GetRHIBuffer(destination.handle);
    cmdList->Copy(sourceRHI, destinationRHI, bufferToTextureCopyDescriptions);
}

void CommandContext::EnqueueDataUpload(const Buffer& buffer, Span<const byte> data, const BufferRegion& region)
{
    if (region == BufferRegion::FullBuffer())
    {
        // Error out if data does not have the same byte size as the buffer.
        // We prefer an explicit subresource for partial uploads to better diagnose mistakes.
        VEX_CHECK(data.size_bytes() == buffer.desc.byteSize,
                  "Passing in no subresource indicates that a total upload is desired. This is not possible since the "
                  "data passed in has a different size to the actual buffer's byteSize.");
    }

    if (buffer.desc.memoryLocality == ResourceMemoryLocality::CPUWrite)
    {
        RHIBuffer& rhiDestBuffer = graphics->GetRHIBuffer(buffer.handle);
        MappedMemory(rhiDestBuffer).WriteData(data, region.offset);
        return;
    }

    BufferUtil::ValidateBufferRegion(buffer.desc, region);

    Buffer stagingBuffer = CreateTemporaryStagingBuffer(buffer.desc.name, region.GetByteSize(buffer.desc));

    RHIBuffer& rhiStagingBuffer = graphics->GetRHIBuffer(stagingBuffer.handle);
    MappedMemory(rhiStagingBuffer).WriteData({ data.begin(), data.begin() + region.GetByteSize(buffer.desc) });

    Copy(stagingBuffer,
         buffer,
         BufferCopyDesc{
             .srcOffset = 0,
             .dstOffset = region.offset,
             .byteSize = region.GetByteSize(buffer.desc),
         });
}

void CommandContext::EnqueueDataUpload(const Texture& texture,
                                       Span<const byte> packedData,
                                       Span<const TextureRegion> textureRegions)
{
    for (const auto& region : textureRegions)
    {
        TextureUtil::ValidateRegion(texture.desc, region);
    }

    // Validate that the upload regions match the raw data passed in.
    u64 packedDataByteSize = TextureUtil::ComputePackedTextureDataByteSize(texture.desc, textureRegions);
    VEX_CHECK(packedData.size_bytes() == packedDataByteSize,
              "Cannot enqueue a data upload: The passed in packed data's size ({}) must be equal to the total texture "
              "size computed from your specified upload regions ({}). Make sure you are correctly uploading the data "
              "you are specifying in your texture regions and that your data is tightly packed (no alignement "
              "per-mip/per-slice/...).",
              packedData.size_bytes(),
              packedDataByteSize);

    // Create aligned staging buffer.
    u64 stagingBufferByteSize = TextureUtil::ComputeAlignedUploadBufferByteSize(texture.desc, textureRegions);
    Buffer stagingBuffer = CreateTemporaryStagingBuffer(texture.desc.name, stagingBufferByteSize);
    RHIBuffer& rhiStagingBuffer = graphics->GetRHIBuffer(stagingBuffer.handle);

    // The staging buffer has to respect the alignment that which Vex uses for uploads.
    // We suppose however that user data is tightly packed.
    Span<byte> stagingBufferData = rhiStagingBuffer.GetMappedData();
    TextureCopyUtil::WriteTextureDataAligned(texture.desc, textureRegions, packedData, stagingBufferData);

    if (textureRegions.empty())
    {
        Copy(stagingBuffer, texture);
    }
    else
    {
        std::vector<BufferTextureCopyDesc> bufferToTexDescs =
            CommandContext_Internal::GetBufferTextureCopyDescFromTextureRegions(texture.desc, textureRegions);
        Copy(stagingBuffer, texture, bufferToTexDescs);
    }
}

void CommandContext::EnqueueDataUpload(const Texture& texture,
                                       Span<const byte> packedData,
                                       const TextureRegion& textureRegion)
{
    EnqueueDataUpload(texture, packedData, { &textureRegion, 1 });
}

TextureReadbackContext CommandContext::EnqueueDataReadback(const Texture& srcTexture,
                                                           Span<const TextureRegion> textureRegions)
{
    for (const auto& region : textureRegions)
    {
        TextureUtil::ValidateRegion(srcTexture.desc, region);
    }

    // Create packed readback buffer.
    u64 stagingBufferByteSize = TextureUtil::ComputeAlignedUploadBufferByteSize(srcTexture.desc, textureRegions);
    const BufferDesc readbackBufferDesc =
        BufferDesc::CreateReadbackBufferDesc(srcTexture.desc.name + "_readback", stagingBufferByteSize);
    Buffer stagingBuffer = graphics->CreateBuffer(readbackBufferDesc, ResourceLifetime::Static);

    if (textureRegions.empty())
    {
        Copy(srcTexture, stagingBuffer);
    }
    else
    {
        Copy(srcTexture,
             stagingBuffer,
             CommandContext_Internal::GetBufferTextureCopyDescFromTextureRegions(srcTexture.desc, textureRegions));
    }

    return { stagingBuffer, textureRegions, srcTexture.desc, *graphics };
}

TextureReadbackContext CommandContext::EnqueueDataReadback(const Texture& srcTexture,
                                                           const TextureRegion& textureRegion)
{
    return EnqueueDataReadback(srcTexture, { &textureRegion, 1 });
}

void CommandContext::BuildBLAS(const AccelerationStructure& accelerationStructure, const BLASBuildDesc& desc)
{
    VEX_CHECK(GPhysicalDevice->IsFeatureSupported(Feature::RayTracing),
              "Your GPU does not support ray tracing, unable to build BLAS!");
    VEX_CHECK(accelerationStructure.handle.IsValid(), "Provided acceleration structure must be valid!");
    VEX_CHECK(accelerationStructure.desc.type == ASType::BottomLevel,
              "BuildBLAS only accepts bottom level acceleration structures...");
    VEX_CHECK(!desc.geometry.empty(), "Cannot build an empty BLAS...");

    std::vector<RHIBLASGeometryDesc> rhiBLASGeometryDescs;
    rhiBLASGeometryDescs.reserve(desc.geometry.size());

    if (desc.type == ASGeometryType::Triangles)
    {
        for (const BLASGeometryDesc& desc : desc.geometry)
        {
            if (desc.indexBufferBinding)
            {
                VEX_CHECK(desc.indexBufferBinding->buffer.desc.usage & BufferUsage::BuildAccelerationStructure,
                          "Index buffer binding must have the BuildAccelerationStructure usage to be used as source "
                          "for building a BLAS");
            }
            VEX_CHECK(desc.vertexBufferBinding.buffer.desc.usage & BufferUsage::BuildAccelerationStructure,
                      "Vertex buffer binding must have the BuildAccelerationStructure usage to be used as source for "
                      "building a BLAS");
        }

        // Upload all the transforms into one GPU/ staging buffer.
        std::vector<std::array<float, 3 * 4>> transformsToUpload;
        // Conservative allocation.
        transformsToUpload.reserve(desc.geometry.size());
        for (const BLASGeometryDesc& blasGeometry : desc.geometry)
        {
            if (blasGeometry.transform.has_value())
            {
                transformsToUpload.push_back(*blasGeometry.transform);
            }
        }

        static constexpr u64 TransformMatrixSize = sizeof(float) * 3 * 4;

        // Upload transform data.
        Buffer transformBuffer;
        if (!transformsToUpload.empty())
        {
            transformBuffer = CreateTemporaryStagingBuffer(
                graphics->GetRHIAccelerationStructure(accelerationStructure.handle).GetDesc().name +
                    "_build_blas_transforms",
                transformsToUpload.size() * TransformMatrixSize);

            MappedMemory mappedMemory = graphics->MapResource(transformBuffer);
            mappedMemory.WriteData(std::as_bytes(std::span<std::array<float, 3 * 4>>(transformsToUpload)));
        }

        // Ensure that vertex/index buffers have had time to be written-to correctly.
        EnqueueGlobalBarrier({
            .srcSync = RHIBarrierSync::AllCommands,
            .dstSync = RHIBarrierSync::BuildAccelerationStructure,
            .srcAccess = RHIBarrierAccess::MemoryWrite,
            .dstAccess = RHIBarrierAccess::ShaderRead,
        });

        u32 transformIndex = 0;
        for (const BLASGeometryDesc& blasGeometry : desc.geometry)
        {
            // TODO(https://trello.com/c/srGndUSP): Handle other vertex formats, this should be cross-referenced
            // with Vulkan to make sure only formats supported by both APIs are accepted.
            if (*blasGeometry.vertexBufferBinding.strideByteSize > sizeof(float) * 3)
            {
                VEX_LOG(
                    Warning,
                    "Vex currently does not support acceleration structure geometry whose vertices have a format "
                    "different to 12 bytes (RGB32). Your vertex buffer binding has a different stride than this, this "
                    "is ok as long as the user is aware that elements outside the first 12 bytes will be ignored.");
            }

            VEX_ASSERT(*blasGeometry.vertexBufferBinding.strideByteSize >= sizeof(float) * 3,
                       "Vex currently does not support acceleration structure geometry whose vertices have a stride "
                       "smaller than 12 bytes.");

            RHIBLASGeometryDesc rhiBLASGeometry{
                .vertexBufferBinding =
                    RHIBufferBinding(blasGeometry.vertexBufferBinding,
                                     graphics->GetRHIBuffer(blasGeometry.vertexBufferBinding.buffer.handle)),
                .flags = blasGeometry.flags,
            };

            if (blasGeometry.indexBufferBinding.has_value())
            {
                VEX_CHECK(blasGeometry.indexBufferBinding->strideByteSize == sizeof(u32),
                          "Vex only supports 32bit index types")
                rhiBLASGeometry.indexBufferBinding =
                    RHIBufferBinding(*blasGeometry.indexBufferBinding,
                                     graphics->GetRHIBuffer(blasGeometry.indexBufferBinding->buffer.handle));
            }

            if (blasGeometry.transform.has_value())
            {
                rhiBLASGeometry.transformBufferBinding = RHIBufferBinding{
                    .binding = { .buffer = transformBuffer,
                                 .offsetByteSize = TransformMatrixSize * transformIndex,
                                 .rangeByteSize = TransformMatrixSize },
                    .buffer = graphics->GetRHIBuffer(transformBuffer.handle),
                };
                ++transformIndex;
            }

            rhiBLASGeometryDescs.push_back(std::move(rhiBLASGeometry));
        }
    }
    else if (desc.type == ASGeometryType::AABBs)
    {
        // Upload all the AABBs into one GPU/ staging buffer.
        std::vector<AABB> aabbsToUpload;
        // Conservative allocation.
        aabbsToUpload.reserve(desc.geometry.size());
        for (const BLASGeometryDesc& blasGeometry : desc.geometry)
        {
            for (const AABB& aabb : blasGeometry.aabbs)
            {
                aabbsToUpload.push_back(aabb);
            }
        }

        // Upload AABB data.
        Buffer aabbBuffer;
        if (!aabbsToUpload.empty())
        {
            aabbBuffer = CreateTemporaryStagingBuffer(
                graphics->GetRHIAccelerationStructure(accelerationStructure.handle).GetDesc().name + "_build_blas_aabb",
                aabbsToUpload.size() * sizeof(AABB));

            MappedMemory mappedMemory = graphics->MapResource(aabbBuffer);
            mappedMemory.WriteData(std::as_bytes(std::span(aabbsToUpload)));
        }

        // Ensure that AABB upload is finished.
        EnqueueGlobalBarrier(RHIGlobalBarrier{
            .srcSync = RHIBarrierSync::AllCommands,
            .dstSync = RHIBarrierSync::BuildAccelerationStructure,
            .srcAccess = RHIBarrierAccess::MemoryWrite,
            .dstAccess = RHIBarrierAccess::ShaderRead,
        });

        u32 aabbIndex = 0;
        for (const BLASGeometryDesc& blasGeometry : desc.geometry)
        {
            RHIBLASGeometryDesc rhiBLASGeometry{
                .aabbBufferBinding =
                    RHIBufferBinding{
                        .binding = { .buffer = aabbBuffer,
                                     .strideByteSize = static_cast<u32>(sizeof(AABB)),
                                     .offsetByteSize = sizeof(AABB) * aabbIndex,
                                     .rangeByteSize = sizeof(AABB) * blasGeometry.aabbs.size() },
                        .buffer = graphics->GetRHIBuffer(aabbBuffer.handle),
                    },
                .flags = blasGeometry.flags,
            };
            aabbIndex += blasGeometry.aabbs.size();
            rhiBLASGeometryDescs.push_back(std::move(rhiBLASGeometry));
        }
    }
    else
    {
        VEX_CHECK(false, "Invalid geometry type passed for BLAS building...You must use either AABB or triangles.");
    }

    const RHIAccelerationStructureBuildInfo& buildInfo =
        graphics->GetRHIAccelerationStructure(accelerationStructure.handle)
            .SetupBLASBuild(*graphics->allocator,
                            RHIBLASBuildDesc{ .type = desc.type, .geometries = rhiBLASGeometryDescs });

    BufferDesc scratchBufferDesc{
        .name =
            graphics->GetRHIAccelerationStructure(accelerationStructure.handle).GetDesc().name + "_build_blas_scratch",
        .byteSize = buildInfo.scratchByteSize,
        .usage = BufferUsage::Scratch,
    };
    Buffer scratchBuffer = CreateTemporaryBuffer(scratchBufferDesc);

    FlushBarriers();
    cmdList->BuildBLAS(graphics->GetRHIAccelerationStructure(accelerationStructure.handle),
                       graphics->GetRHIBuffer(scratchBuffer.handle));
}

void CommandContext::BuildTLAS(const AccelerationStructure& accelerationStructure, const TLASBuildDesc& desc)
{
    VEX_CHECK(GPhysicalDevice->IsFeatureSupported(Feature::RayTracing),
              "Your GPU does not support ray tracing, unable to build TLAS!");
    VEX_CHECK(accelerationStructure.handle.IsValid(), "Provided acceleration structure must be valid!");
    VEX_CHECK(accelerationStructure.desc.type == ASType::TopLevel,
              "BuildTLAS only accepts top level acceleration structures...");
    VEX_CHECK(!desc.instances.empty(), "Cannot build an empty TLAS...");

    std::unordered_set<AccelerationStructure> uniqueBLAS;
    std::vector<NonNullPtr<RHIAccelerationStructure>> perInstanceBLAS;
    perInstanceBLAS.reserve(desc.instances.size());
    for (const TLASInstanceDesc& tlasInstanceDesc : desc.instances)
    {
        perInstanceBLAS.push_back(graphics->GetRHIAccelerationStructure(tlasInstanceDesc.blas.handle));
        uniqueBLAS.insert(tlasInstanceDesc.blas);
    }

    EnqueueGlobalBarrier(RHIGlobalBarrier{
        .srcSync = RHIBarrierSync::BuildAccelerationStructure,
        .dstSync = RHIBarrierSync::BuildAccelerationStructure,
        .srcAccess = RHIBarrierAccess::AccelerationStructureWrite,
        .dstAccess = RHIBarrierAccess::AccelerationStructureRead,
    });

    RHITLASBuildDesc rhiTLASDesc{
        .instances = desc.instances,
        .perInstanceBLAS = perInstanceBLAS,
    };

    RHIAccelerationStructure& accelStruct = graphics->GetRHIAccelerationStructure(accelerationStructure.handle);
    std::vector<std::byte> instanceData = accelStruct.GetInstanceBufferData(rhiTLASDesc);

    Buffer instanceBuffer = CreateTemporaryBuffer({
        .name = accelStruct.GetDesc().name + "_build_tlas_instances",
        .byteSize = instanceData.size(),
        .usage = BufferUsage::BuildAccelerationStructure,
    });
    RHIBuffer& rhiInstanceBuffer = graphics->GetRHIBuffer(instanceBuffer.handle);
    EnqueueDataUpload(instanceBuffer, instanceData);

    BufferBinding binding =
        BufferBinding::CreateStructuredBuffer(instanceBuffer, accelStruct.GetInstanceBufferStride());
    rhiTLASDesc.instancesBinding = RHIBufferBinding{ binding, rhiInstanceBuffer };

    const RHIAccelerationStructureBuildInfo& buildInfo = accelStruct.SetupTLASBuild(*graphics->allocator, rhiTLASDesc);
    Buffer scratchBuffer = CreateTemporaryBuffer({
        .name = accelStruct.GetDesc().name + "_build_tlas_scratch",
        .byteSize = buildInfo.scratchByteSize,
        .usage = BufferUsage::Scratch,
    });

    EnqueueGlobalBarrier({ .srcSync = RHIBarrierSync::Copy,
                           .dstSync = RHIBarrierSync::BuildAccelerationStructure,
                           .srcAccess = RHIBarrierAccess::CopyDest,
                           .dstAccess = RHIBarrierAccess::MemoryRead });

    FlushBarriers();
    cmdList->BuildTLAS(accelStruct,
                       graphics->GetRHIBuffer(scratchBuffer.handle),
                       graphics->GetRHIBuffer(instanceBuffer.handle),
                       rhiTLASDesc);
}

BufferReadbackContext CommandContext::EnqueueDataReadback(const Buffer& srcBuffer, const BufferRegion& region)
{
    BufferUtil::ValidateBufferRegion(srcBuffer.desc, region);

    // Create packed readback buffer.
    const BufferDesc readbackBufferDesc =
        BufferDesc::CreateReadbackBufferDesc(srcBuffer.desc.name + "_readback", region.GetByteSize(srcBuffer.desc));
    Buffer stagingBuffer = graphics->CreateBuffer(readbackBufferDesc, ResourceLifetime::Static);

    if (srcBuffer.desc.byteSize == GBufferWholeSize)
    {
        Copy(srcBuffer, stagingBuffer);
    }
    else
    {
        Copy(srcBuffer, stagingBuffer, BufferCopyDesc{ region.offset, 0, region.GetByteSize(srcBuffer.desc) });
    }

    return { stagingBuffer, *graphics };
}

void CommandContext::ExecuteInDrawContext(Span<const TextureBinding> renderTargets,
                                          std::optional<const TextureBinding> depthStencil,
                                          Span<const ResourceBinding> trackedResources,
                                          const std::function<void()>& callback)
{
    RHIDrawResources drawResources =
        ResourceBindingUtils::CollectRHIDrawResources(*graphics, renderTargets, depthStencil);
    for (const auto& rt : drawResources.renderTargets)
    {
        EnqueueTextureBarrier(rt.binding.texture,
                              rt.binding.subresource,
                              RHIBarrierSync::RenderTarget,
                              RHIBarrierAccess::RenderTarget,
                              RHITextureLayout::RenderTarget);
    }
    if (drawResources.depthStencil.has_value())
    {
        // Use the most restrictive access, since we don't know how the caller will use the depth stencil texture.
        RHIBarrierAccess depthAccess = RHIBarrierAccess::DepthStencilReadWrite;
        RHITextureLayout depthLayout = RHITextureLayout::DepthStencilWrite;
        EnqueueTextureBarrier(drawResources.depthStencil->binding.texture,
                              drawResources.depthStencil->binding.subresource,
                              RHIBarrierSync::DepthStencil,
                              depthAccess,
                              depthLayout);
    }

    InferResourceBarriers(RHIBarrierSync::AllGraphics, trackedResources);

    FlushBarriers();
    cmdList->BeginRendering(drawResources);
    callback();
    cmdList->EndRendering();
}

QueryHandle CommandContext::BeginTimestampQuery()
{
    cmdList->EmitBarriers({},
                          {},
                          { RHIGlobalBarrier{
                              RHIBarrierSync::Copy,
                              RHIBarrierSync::Copy,
                              RHIBarrierAccess::CopyDest,
                              RHIBarrierAccess::CopySource,
                          } });
    return cmdList->BeginTimestampQuery();
}

void CommandContext::EndTimestampQuery(QueryHandle handle)
{
    cmdList->EndTimestampQuery(handle);
}

void CommandContext::Barrier(const Buffer& buffer, RHIBarrierAccess access)
{
    EnqueueGlobalBarrier({
        .srcSync = RHIBarrierSync::AllCommands,
        .dstSync = RHIBarrierSync::AllCommands,
        .srcAccess = RHIBarrierAccess::MemoryWrite,
        .dstAccess = access,
    });
}

void CommandContext::Barrier(const Texture& texture, RHIBarrierAccess access, const TextureSubresource& subresource)
{
    EnqueueTextureBarrier(texture, subresource, RHIBarrierSync::AllCommands, access, RHIAccessToRHILayout(access));
}

void CommandContext::Barrier(const AccelerationStructure& as, RHIBarrierAccess access)
{
    VEX_CHECK(
        access == RHIBarrierAccess::AccelerationStructureRead || access == RHIBarrierAccess::AccelerationStructureWrite,
        "An acceleration structure can only have Acceleration structure accesses.");
    EnqueueGlobalBarrier({
        .srcSync = RHIBarrierSync::BuildAccelerationStructure,
        .dstSync = RHIBarrierSync::AllCommands,
        .srcAccess = RHIBarrierAccess::AccelerationStructureWrite,
        .dstAccess = access,
    });
}

RHICommandList& CommandContext::GetRHICommandList()
{
    return *cmdList;
}

TextureStateMap& CommandContext::GetOrFetchTextureState(TextureHandle handle)
{
    auto [it, inserted] = textureStates.try_emplace(handle, TextureStateMap{});
    if (inserted)
    {
        it->second.SetUniform({ RHIBarrierSync::None, RHIBarrierAccess::NoAccess, RHITextureLayout::Common });
    }
    return it->second;
}

void CommandContext::FlushBarriers()
{
    if (pendingBufferBarriers.empty() && pendingTextureBarriers.empty() && pendingGlobalBarriers.empty())
    {
        return;
    }

    cmdList->EmitBarriers(pendingBufferBarriers, pendingTextureBarriers, pendingGlobalBarriers);

    pendingBufferBarriers.clear();
    pendingTextureBarriers.clear();
    pendingGlobalBarriers.clear();
}

void CommandContext::EnqueueTextureBarrier(const Texture& texture,
                                           const TextureSubresource& subresource,
                                           RHIBarrierSync dstSync,
                                           RHIBarrierAccess dstAccess,
                                           RHITextureLayout dstLayout)
{
    TextureStateMap& textureStateMap = GetOrFetchTextureState(texture.handle);

    // Iterate on subresource sections which have the same source state.
    textureStateMap.ForEachStateSection(texture.desc,
                                        subresource,
                                        [&](const TextureSubresource& section, RHITextureState srcState)
                                        {
                                            RHITextureBarrier barrier{
                                                .texture = graphics->GetRHITexture(texture.handle),
                                                .subresource = section,
                                                .srcSync = srcState.sync,
                                                .dstSync = dstSync,
                                                .srcAccess = srcState.access,
                                                .dstAccess = dstAccess,
                                                .srcLayout = srcState.layout,
                                                .dstLayout = dstLayout,
                                            };

                                            const bool sameState = srcState.sync == dstSync &&
                                                                   srcState.access == dstAccess &&
                                                                   srcState.layout == dstLayout;

                                            const bool isDstWriteAccess = IsWriteAccess(dstAccess);

                                            // Already in the right state, nothing to do. If the dest is a write, we
                                            // still have to add the barrier though to avoid Write-After-Write hazards.
                                            if (sameState && !isDstWriteAccess)
                                            {
                                                return;
                                            }

                                            // Have to add all resources to the touched textures list, as DX12 could
                                            // require a resource layout transition (even when read<->read is
                                            // occurring).
                                            touchedTextures.insert(texture);

                                            pendingTextureBarriers.push_back(std::move(barrier));
                                        });

    // Set the new state in the texture state map, no matter the previous codepath we end up with the entire passed in
    // subresource in a uniform state.
    textureStateMap.Set(texture.desc, subresource, { dstSync, dstAccess, dstLayout });
}

void CommandContext::EnqueueGlobalBarrier(const RHIGlobalBarrier& globalBarrier)
{
    pendingGlobalBarriers.push_back(globalBarrier);
}

void CommandContext::InferResourceBarriers(RHIBarrierSync syncStage, Span<const ResourceBinding> resources)
{
    for (const auto& rb : resources)
    {
        std::visit(
            Visitor{ [this, syncStage](const BufferBinding& bufferBinding)
                     {
                         using enum BufferBindingUsage;
                         RHIBarrierAccess dstAccess{};
                         switch (bufferBinding.usage)
                         {
                         case UniformBuffer:
                             dstAccess = RHIBarrierAccess::UniformRead;
                             break;
                         case StructuredBuffer:
                         case ByteAddressBuffer:
                             dstAccess = RHIBarrierAccess::ShaderRead;
                             break;
                         case RWStructuredBuffer:
                         case RWByteAddressBuffer:
                             dstAccess = RHIBarrierAccess::ShaderReadWrite;
                             break;
                         default:
                             VEX_LOG(Fatal, "Invalid usage for buffer binding {}...", bufferBinding.buffer.desc.name);
                             break;
                         }

                         // In Vex buffers just use global barriers.
                         EnqueueGlobalBarrier(RHIGlobalBarrier{
                             .srcSync = RHIBarrierSync::AllCommands,
                             .dstSync = syncStage,
                             .srcAccess = RHIBarrierAccess::MemoryWrite,
                             .dstAccess = dstAccess,
                         });
                     },
                     [this, syncStage](const TextureBinding& texBinding)
                     {
                         using enum TextureBindingUsage;
                         RHIBarrierAccess access;
                         RHITextureLayout layout;
                         switch (texBinding.usage)
                         {
                         case ShaderRead:
                             access = RHIBarrierAccess::ShaderRead;
                             layout = RHITextureLayout::ShaderRead;
                             break;
                         case ShaderReadWrite:
                             access = RHIBarrierAccess::ShaderReadWrite;
                             layout = RHITextureLayout::ShaderReadWrite;
                             break;
                         default:
                             VEX_LOG(Fatal, "Invalid usage for texture binding {}...", texBinding.texture.desc.name);
                             break;
                         }

                         EnqueueTextureBarrier(texBinding.texture, texBinding.subresource, syncStage, access, layout);
                     },
                     [this, syncStage](const AccelerationStructureBinding& asBinding)
                     {
                         // All ASBindings use AccelerationStructureRead.
                         EnqueueGlobalBarrier(RHIGlobalBarrier{
                             .srcSync = RHIBarrierSync::AllCommands,
                             .dstSync = syncStage,
                             .srcAccess = RHIBarrierAccess::MemoryWrite,
                             .dstAccess = RHIBarrierAccess::AccelerationStructureRead,
                         });
                     } },
            rb.binding);
    }
}

ScopedGPUEvent CommandContext::CreateScopedGPUEvent(const char* markerLabel, std::array<float, 3> color)
{
    VEX_CHECK(cmdList->IsOpen(), "Cannot create a scoped GPU Event with a closed command context.");
    return { cmdList->CreateScopedMarker(markerLabel, color) };
}

Buffer CommandContext::CreateTemporaryStagingBuffer(const std::string& name,
                                                    u64 byteSize,
                                                    BufferUsage::Flags additionalUsages)
{
    const Buffer stagingBuffer =
        graphics->CreateBuffer(BufferDesc::CreateStagingBufferDesc(name + "_staging", byteSize, additionalUsages));
    // Schedule a cleanup of the staging buffer.
    temporaryBuffers.push_back(stagingBuffer);
    return stagingBuffer;
}

Buffer CommandContext::CreateTemporaryBuffer(const BufferDesc& desc)
{
    const Buffer buf = graphics->CreateBuffer(desc);
    // Schedule a cleanup of the buffer.
    temporaryBuffers.push_back(buf);
    return buf;
}

std::optional<RHIDrawResources> CommandContext::PrepareDrawCall(const DrawDesc& drawDesc,
                                                                const DrawResourceBinding& drawBindings,
                                                                ConstantBinding constants,
                                                                Span<const ResourceBinding> trackedResources)
{
    InferResourceBarriers(RHIBarrierSync::AllGraphics, trackedResources);

    // Transition RTs/DepthStencil
    RHIDrawResources drawResources = ResourceBindingUtils::CollectRHIDrawResources(*graphics,
                                                                                   drawBindings.renderTargets,
                                                                                   drawBindings.depthStencil,
                                                                                   drawDesc.depthStencilState);
    for (const auto& rt : drawResources.renderTargets)
    {
        EnqueueTextureBarrier(rt.binding.texture,
                              rt.binding.subresource,
                              RHIBarrierSync::RenderTarget,
                              RHIBarrierAccess::RenderTarget,
                              RHITextureLayout::RenderTarget);
    }
    if (drawResources.depthStencil.has_value())
    {
        // Start with the most restrictive access.
        RHIBarrierAccess depthAccess;
        if (!drawDesc.depthStencilState.depthWriteEnabled)
        {
            depthAccess = RHIBarrierAccess::DepthStencilRead;
        }
        else
        {
            depthAccess = drawDesc.depthStencilState.depthTestEnabled ? RHIBarrierAccess::DepthStencilReadWrite
                                                                      : RHIBarrierAccess::DepthStencilWrite;
        }
        RHITextureLayout depthLayout = drawDesc.depthStencilState.depthWriteEnabled
                                           ? RHITextureLayout::DepthStencilWrite
                                           : RHITextureLayout::DepthStencilRead;

        EnqueueTextureBarrier(drawResources.depthStencil->binding.texture,
                              drawResources.depthStencil->binding.subresource,
                              RHIBarrierSync::DepthStencil,
                              depthAccess,
                              depthLayout);
    }

    auto graphicsPSOKey = CommandContext_Internal::GetGraphicsPSOKeyFromDrawDesc(drawDesc, drawResources);

    if (!cachedGraphicsPSOKey || graphicsPSOKey != *cachedGraphicsPSOKey)
    {
        std::unique_ptr<RHIGraphicsPipelineState> oldPSO;
        const RHIGraphicsPipelineState* pipelineState =
            graphics->psCache->GetGraphicsPipelineState(graphicsPSOKey, oldPSO);
        if (oldPSO)
        {
            temporaryResources.emplace_back(std::move(oldPSO));
        }

        // No valid PSO means we cannot proceed.
        if (!pipelineState)
        {
            return std::nullopt;
        }

        cmdList->SetPipelineState(*pipelineState);
        cachedGraphicsPSOKey = graphicsPSOKey;
    }

    // Setup the layout for our pass.
    RHIResourceLayout& resourceLayout = graphics->psCache->GetResourceLayout();
    resourceLayout.SetLayoutResources(constants);

    cmdList->SetLayout(resourceLayout);

    if (!cachedInputAssembly || drawDesc.inputAssembly != cachedInputAssembly)
    {
        cmdList->SetInputAssembly(drawDesc.inputAssembly);
        cachedInputAssembly = drawDesc.inputAssembly;
    }

    EnqueueGlobalBarrier({ .srcSync = RHIBarrierSync::AllCommands,
                           .dstSync = RHIBarrierSync::AllGraphics,
                           .srcAccess = RHIBarrierAccess::MemoryWrite,
                           .dstAccess = RHIBarrierAccess::VertexInputRead });

    // Bind Vertex Buffer(s)
    SetVertexBuffers(drawBindings.vertexBuffersFirstSlot, drawBindings.vertexBuffers);

    // Bind Index Buffer.
    if (drawBindings.indexBuffer.has_value())
    {
        SetIndexBuffer(*drawBindings.indexBuffer);
    }

    return drawResources;
}

void CommandContext::CheckViewportAndScissor() const
{
    // Graphics APIs require the viewport and scissor rect to be initialized before performing Graphics-Queue related
    // operations. Keep track of this so that the user doesn't forget. We do not want to set it automatically since
    // using the present texture's size is imprecise due to window resize being possible (additionally the user might
    // not be using a swapchain).
    VEX_CHECK(hasInitializedViewport,
              "No viewport was set! Remember to call CommandContext::SetViewport before performing a draw call!");
    VEX_CHECK(hasInitializedScissor,
              "No scissor rect was set! Remember to call CommandContext::SetScissor before performing a draw call!");
}

void CommandContext::SetVertexBuffers(u32 vertexBuffersFirstSlot, Span<const BufferBinding> vertexBuffers)
{
    if (vertexBuffers.empty())
    {
        return;
    }

    std::vector<RHIBufferBinding> rhiBindings;
    rhiBindings.reserve(vertexBuffers.size());
    for (const auto& vertexBuffer : vertexBuffers)
    {
        if (!vertexBuffer.strideByteSize.has_value())
        {
            VEX_LOG(Fatal, "A vertex buffer must have a valid strideByteSize!");
        }
        RHIBuffer& buffer = graphics->GetRHIBuffer(vertexBuffer.buffer.handle);
        rhiBindings.emplace_back(vertexBuffer, NonNullPtr(buffer));
    }
    cmdList->SetVertexBuffers(vertexBuffersFirstSlot, rhiBindings);
}

void CommandContext::SetIndexBuffer(BufferBinding indexBuffer)
{
    RHIBuffer& buffer = graphics->GetRHIBuffer(indexBuffer.buffer.handle);
    RHIBufferBinding binding{ indexBuffer, NonNullPtr(buffer) };
    cmdList->SetIndexBuffer(binding);
}

} // namespace vex