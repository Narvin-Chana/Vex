#include "CommandContext.h"

#include <algorithm>
#include <cmath>
#include <variant>

#include <Vex/ByteUtils.h>
#include <Vex/Containers/Utils.h>
#include <Vex/Debug.h>
#include <Vex/DrawHelpers.h>
#include <Vex/GfxBackend.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/RHIImpl/RHISwapChain.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/RayTracing.h>
#include <Vex/ResourceBindingUtils.h>
#include <Vex/Validation.h>

#include <RHI/RHIBarrier.h>
#include <RHI/RHIBindings.h>

namespace vex
{

namespace CommandContext_Internal
{

static std::vector<RHIBufferBarrier> CreateBarriersFromBindings(const std::vector<RHIBufferBinding>& rhiBufferBindings)
{
    std::vector<RHIBufferBarrier> barriers;
    barriers.reserve(rhiBufferBindings.size());
    for (const auto& rhiBinding : rhiBufferBindings)
    {
        barriers.push_back(ResourceBindingUtils::CreateBarrierFromRHIBinding(rhiBinding));
    }
    return barriers;
}

static std::vector<RHITextureBarrier> CreateBarriersFromBindings(
    const std::vector<RHITextureBinding>& rhiTextureBindings)
{
    std::vector<RHITextureBarrier> barriers;
    barriers.reserve(rhiTextureBindings.size());
    for (const auto& rhiBinding : rhiTextureBindings)
    {
        barriers.push_back(ResourceBindingUtils::CreateBarrierFromRHIBinding(rhiBinding));
    }
    return barriers;
}

static GraphicsPipelineStateKey GetGraphicsPSOKeyFromDrawDesc(const DrawDescription& drawDesc,
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
        key.renderTargetState.colorFormats.emplace_back(rhiBinding.binding.texture.description.format);
    }

    if (rhiDrawRes.depthStencil)
    {
        key.renderTargetState.depthStencilFormat = rhiDrawRes.depthStencil->binding.texture.description.format;
    }

    // Ensure each render target has atleast a default color attachment (no blending, write all).
    key.colorBlendState.attachments.resize(rhiDrawRes.renderTargets.size());

    return key;
}

static BufferDescription GetStagingBufferDescription(const std::string& name, u64 byteSize)
{
    BufferDescription description;
    description.byteSize = byteSize;
    description.name = std::string(name) + "_staging";
    description.usage = BufferUsage::None;
    description.memoryLocality = ResourceMemoryLocality::CPUWrite;
    return description;
}

static void UploadTextureDataAligned(const TextureDescription& textureDesc,
                                     std::span<const TextureUploadRegion> uploadRegions,
                                     std::span<const byte> packedData,
                                     std::span<byte> stagingBuffer)

{
    const u32 bytesPerPixel = TextureUtil::GetPixelByteSizeFromFormat(textureDesc.format);
    const byte* srcData = packedData.data();
    byte* dstData = stagingBuffer.data();
    u64 srcOffset = 0;
    u64 dstOffset = 0;

    for (const TextureUploadRegion& region : uploadRegions)
    {
        const u32 regionWidth = region.extent.width;
        const u32 regionHeight = region.extent.height;
        const u32 regionDepth = region.extent.depth;

        const u32 packedRowPitch = regionWidth * bytesPerPixel;
        const u32 alignedRowPitch = AlignUp<u32>(packedRowPitch, TextureUtil::RowPitchAlignment);
        const u32 packedSlicePitch = packedRowPitch * regionHeight;
        const u32 alignedSlicePitch = alignedRowPitch * regionHeight;

        // Copy each depth slice (for 3D textures).
        for (u32 depthSlice = 0; depthSlice < regionDepth; ++depthSlice)
        {
            // Copy each row one-by-one with alignment.
            for (u32 row = 0; row < regionHeight; ++row)
            {
                const byte* srcRow = srcData + srcOffset + depthSlice * packedSlicePitch + row * packedRowPitch;
                byte* dstRow = dstData + dstOffset + depthSlice * alignedSlicePitch + row * alignedRowPitch;

                std::memcpy(dstRow, srcRow, packedRowPitch);
#if !VEX_SHIPPING
                // Zero out padding bytes for debugging purposes.
                if (alignedRowPitch > packedRowPitch)
                {
                    std::memset(dstRow + packedRowPitch, 0, alignedRowPitch - packedRowPitch);
                }
#endif
            }
        }

        // Move to next region in the packed source data.
        srcOffset += static_cast<u64>(packedSlicePitch) * regionDepth;

        // Move to next aligned position in staging buffer.
        u64 regionStagingSize = static_cast<u64>(alignedSlicePitch) * regionDepth;
        dstOffset += AlignUp<u64>(regionStagingSize, TextureUtil::MipAlignment);
    }
}

} // namespace CommandContext_Internal

CommandContext::CommandContext(NonNullPtr<GfxBackend> backend,
                               NonNullPtr<RHICommandList> cmdList,
                               SubmissionPolicy submissionPolicy,
                               std::span<SyncToken> dependencies)
    : backend(backend)
    , cmdList(cmdList)
    , submissionPolicy(submissionPolicy)
    , dependencies{ dependencies.begin(), dependencies.end() }
{
    cmdList->Open();
    if (cmdList->GetType() != CommandQueueType::Copy)
    {
        cmdList->SetDescriptorPool(*backend->descriptorPool, backend->psCache.GetResourceLayout());
    }
}

CommandContext::~CommandContext()
{
    if (hasSubmitted)
    {
        return;
    }

    backend->EndCommandContext(*this);
}

void CommandContext::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
    cmdList->SetViewport(x, y, width, height, minDepth, maxDepth);
}

void CommandContext::SetScissor(i32 x, i32 y, u32 width, u32 height)
{
    cmdList->SetScissor(x, y, width, height);
}

void CommandContext::ClearTexture(const TextureBinding& binding,
                                  std::optional<TextureClearValue> textureClearValue,
                                  std::optional<std::array<float, 4>> clearRect)
{
    if (!(binding.texture.description.usage & (TextureUsage::RenderTarget | TextureUsage::DepthStencil)))
    {
        VEX_LOG(Fatal,
                "ClearUsage not supported on this texture, it must be either usable as a render target or as a depth "
                "stencil!");
    }

    if (clearRect.has_value())
    {
        // Clear Rect not yet supported.
        VEX_NOT_YET_IMPLEMENTED();
    }

    RHITexture& texture = backend->GetRHITexture(binding.texture.handle);
    RHITextureBarrier barrier = texture.GetClearTextureBarrier();
    cmdList->Barrier({}, { &barrier, 1 });
    cmdList->ClearTexture({ binding, NonNullPtr(texture) },
                          // This is a safe cast, textures can only contain one of the two usages (RT/DS).
                          static_cast<TextureUsage::Type>(binding.texture.description.usage &
                                                          (TextureUsage::RenderTarget | TextureUsage::DepthStencil)),
                          textureClearValue.value_or(binding.texture.description.clearValue));
}

void CommandContext::Draw(const DrawDescription& drawDesc,
                          const DrawResourceBinding& drawBindings,
                          std::optional<ConstantBinding> constants,
                          u32 vertexCount,
                          u32 instanceCount,
                          u32 vertexOffset,
                          u32 instanceOffset)
{
    // Index buffers are not used in Draw, warn the user if they have still bound one.
    if (drawBindings.indexBuffer.has_value())
    {
        VEX_LOG(Warning,
                "Your CommandContext::Draw call resources contain an index buffer which will be ignored. If you wish "
                "to use the index buffer, call CommandContext::DrawIndexed instead.");
    }

    auto drawResources = PrepareDrawCall(drawDesc, drawBindings, constants);
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

void CommandContext::DrawIndexed(const DrawDescription& drawDesc,
                                 const DrawResourceBinding& drawBindings,
                                 std::optional<ConstantBinding> constants,
                                 u32 indexCount,
                                 u32 instanceCount,
                                 u32 indexOffset,
                                 u32 vertexOffset,
                                 u32 instanceOffset)
{
    auto drawResources = PrepareDrawCall(drawDesc, drawBindings, constants);
    if (!drawResources.has_value())
    {
        return;
    }

    cmdList->BeginRendering(*drawResources);
    // TODO(https://trello.com/c/IGxuLci9): Validate draw index count (eg: versus the currently used index buffer size)
    cmdList->DrawIndexed(indexCount, instanceCount, indexOffset, vertexOffset, instanceOffset);
    cmdList->EndRendering();
}

void CommandContext::Dispatch(const ShaderKey& shader,
                              const std::optional<ConstantBinding>& constants,
                              std::array<u32, 3> groupCount)
{
    if (shader.type != ShaderType::ComputeShader)
    {
        VEX_LOG(Fatal, "Invalid shader type passed to Dispatch call: {}", magic_enum::enum_name(shader.type));
    }

    using namespace CommandContext_Internal;

    ComputePipelineStateKey psoKey = { .computeShader = shader };
    if (!cachedComputePSOKey || psoKey != cachedComputePSOKey)
    {
        // Register shader and get Pipeline if exists (if not create it).
        const RHIComputePipelineState* pipelineState = backend->psCache.GetComputePipelineState(psoKey);

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
    RHIResourceLayout& resourceLayout = backend->psCache.GetResourceLayout();
    resourceLayout.SetLayoutResources(constants);
    cmdList->SetLayout(resourceLayout);

    // Validate dispatch (vs platform/api constraints)
    // backend->ValidateDispatch(groupCount);

    // Perform dispatch
    cmdList->Dispatch(groupCount);
}

void CommandContext::TraceRays(const RayTracingPassDescription& rayTracingPassDescription,
                               const std::optional<ConstantBinding>& constants,
                               std::array<u32, 3> widthHeightDepth)
{
    RayTracingPassDescription::ValidateShaderTypes(rayTracingPassDescription);

    const RHIRayTracingPipelineState* pipelineState =
        backend->psCache.GetRayTracingPipelineState(rayTracingPassDescription, *backend->allocator);
    if (!pipelineState)
    {
        VEX_LOG(Error, "PSO cache returned an invalid pipeline state, unable to continue dispatch...");
        return;
    }
    cmdList->SetPipelineState(*pipelineState);

    // Sets the resource layout to use for the ray trace.
    RHIResourceLayout& resourceLayout = backend->psCache.GetResourceLayout();
    resourceLayout.SetLayoutResources(constants);

    cmdList->SetLayout(resourceLayout);

    // Validate ray trace (vs platform/api constraints)
    // backend->ValidateTraceRays(widthHeightDepth);

    cmdList->TraceRays(widthHeightDepth, *pipelineState);
}

void CommandContext::Copy(const Texture& source, const Texture& destination)
{
    if (source.handle == destination.handle)
    {
        VEX_LOG(Fatal, "Cannot copy a texture to itself!");
    }

    TextureUtil::ValidateCompatibleTextureDescriptions(source.description, destination.description);

    RHITexture& sourceRHI = backend->GetRHITexture(source.handle);
    RHITexture& destinationRHI = backend->GetRHITexture(destination.handle);
    std::array barriers{ RHITextureBarrier{ sourceRHI,
                                            RHIBarrierSync::Copy,
                                            RHIBarrierAccess::CopySource,
                                            RHITextureLayout::CopySource },
                         RHITextureBarrier{ destinationRHI,
                                            RHIBarrierSync::Copy,
                                            RHIBarrierAccess::CopyDest,
                                            RHITextureLayout::CopyDest } };
    cmdList->Barrier({}, barriers);
    cmdList->Copy(sourceRHI, destinationRHI);
}

void CommandContext::Copy(const Texture& source,
                          const Texture& destination,
                          const TextureCopyDescription& regionMapping)
{
    Copy(source, destination, { &regionMapping, 1 });
}

void CommandContext::Copy(const Texture& source,
                          const Texture& destination,
                          std::span<const TextureCopyDescription> regionMappings)
{
    if (source.handle == destination.handle)
    {
        VEX_LOG(Fatal, "Cannot copy a texture to itself!");
    }

    for (auto& mapping : regionMappings)
    {
        TextureUtil::ValidateTextureCopyDescription(source.description, destination.description, mapping);
    }

    RHITexture& sourceRHI = backend->GetRHITexture(source.handle);
    RHITexture& destinationRHI = backend->GetRHITexture(destination.handle);
    std::array barriers{ RHITextureBarrier{ sourceRHI,
                                            RHIBarrierSync::Copy,
                                            RHIBarrierAccess::CopySource,
                                            RHITextureLayout::CopySource },
                         RHITextureBarrier{ destinationRHI,
                                            RHIBarrierSync::Copy,
                                            RHIBarrierAccess::CopyDest,
                                            RHITextureLayout::CopyDest } };
    cmdList->Barrier({}, barriers);
    cmdList->Copy(sourceRHI, destinationRHI, regionMappings);
}

void CommandContext::Copy(const Buffer& source, const Buffer& destination)
{
    if (source.handle == destination.handle)
    {
        VEX_LOG(Fatal, "Cannot copy a buffer to itself!");
    }

    BufferUtil::ValidateSimpleBufferCopy(source.description, destination.description);

    RHIBuffer& sourceRHI = backend->GetRHIBuffer(source.handle);
    RHIBuffer& destinationRHI = backend->GetRHIBuffer(destination.handle);
    std::array barriers{ RHIBufferBarrier{ sourceRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopySource },
                         RHIBufferBarrier{ destinationRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopyDest } };
    cmdList->Barrier(barriers, {});
    cmdList->Copy(sourceRHI, destinationRHI);
}

void CommandContext::Copy(const Buffer& source, const Buffer& destination, const BufferCopyDescription& regionMappings)
{
    if (source.handle == destination.handle)
    {
        VEX_LOG(Fatal, "Cannot copy a buffer to itself!");
    }

    BufferUtil::ValidateBufferCopyDescription(source.description, destination.description, regionMappings);

    RHIBuffer& sourceRHI = backend->GetRHIBuffer(source.handle);
    RHIBuffer& destinationRHI = backend->GetRHIBuffer(destination.handle);
    std::array barriers{ RHIBufferBarrier{ sourceRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopySource },
                         RHIBufferBarrier{ destinationRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopyDest } };
    cmdList->Barrier(barriers, {});

    cmdList->Copy(sourceRHI, destinationRHI, regionMappings);
}

void CommandContext::Copy(const Buffer& source, const Texture& destination)
{
    RHIBuffer& sourceRHI = backend->GetRHIBuffer(source.handle);
    RHITexture& destinationRHI = backend->GetRHITexture(destination.handle);
    RHIBufferBarrier sourceBarrier{ sourceRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopySource };
    RHITextureBarrier destinationBarrier{ destinationRHI,
                                          RHIBarrierSync::Copy,
                                          RHIBarrierAccess::CopyDest,
                                          RHITextureLayout::CopyDest };
    cmdList->Barrier({ &sourceBarrier, 1 }, { &destinationBarrier, 1 });
    cmdList->Copy(sourceRHI, destinationRHI);
}

void CommandContext::Copy(const Buffer& source,
                          const Texture& destination,
                          const BufferToTextureCopyDescription& copyDesc)
{
    Copy(source, destination, { &copyDesc, 1 });
}

void CommandContext::Copy(const Buffer& source,
                          const Texture& destination,
                          std::span<const BufferToTextureCopyDescription> copyDescriptions)
{
    for (auto& copyDesc : copyDescriptions)
    {
        TextureCopyUtil::ValidateBufferToTextureCopyDescription(source.description, destination.description, copyDesc);
    }

    RHIBuffer& sourceRHI = backend->GetRHIBuffer(source.handle);
    RHITexture& destinationRHI = backend->GetRHITexture(destination.handle);
    RHIBufferBarrier sourceBarrier{ sourceRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopySource };
    RHITextureBarrier destinationBarrier{ destinationRHI,
                                          RHIBarrierSync::Copy,
                                          RHIBarrierAccess::CopyDest,
                                          RHITextureLayout::CopyDest };
    cmdList->Barrier({ &sourceBarrier, 1 }, { &destinationBarrier, 1 });

    cmdList->Copy(sourceRHI, destinationRHI, copyDescriptions);
}

void CommandContext::EnqueueDataUpload(const Buffer& buffer,
                                       std::span<const byte> data,
                                       const std::optional<BufferSubresource>& subresource)
{
    bool isFullUpload = !subresource.has_value();
    if (isFullUpload)
    {
        // Error out if data does not have the same byte size as the buffer.
        // We prefer an explicit subresource for partial uploads to better diagnose mistakes.
        VEX_CHECK(data.size_bytes() == buffer.description.byteSize,
                  "Passing in no subresource indicates that a total upload is desired. This is not possible since the "
                  "data passed in has a different size to the actual buffer's byteSize.");
    }

    // Unspecified subresource means we upload the entirety of the data array at offset 0.
    BufferSubresource actualSubresource{
        .offset = !isFullUpload ? subresource->offset : 0,
        .size = !isFullUpload ? subresource->size : data.size_bytes(),
    };

    if (buffer.description.memoryLocality == ResourceMemoryLocality::CPUWrite)
    {
        RHIBuffer& rhiDestBuffer = backend->GetRHIBuffer(buffer.handle);
        ResourceMappedMemory(rhiDestBuffer).SetData(data, actualSubresource.offset);
        return;
    }

    BufferUtil::ValidateBufferSubresource(buffer.description, actualSubresource);

    const BufferDescription stagingBufferDesc =
        CommandContext_Internal::GetStagingBufferDescription(buffer.description.name, actualSubresource.size);

    // Buffer creation invalidates pointers to existing RHI buffers.
    Buffer stagingBuffer = backend->CreateBuffer(stagingBufferDesc, ResourceLifetime::Static);

    // So fetch buffers only AFTER creating the staging buffer.
    RHIBuffer& rhiStagingBuffer = backend->GetRHIBuffer(stagingBuffer.handle);
    RHIBuffer& rhiDestBuffer = backend->GetRHIBuffer(buffer.handle);

    ResourceMappedMemory(rhiStagingBuffer).SetData(data);

    std::array barriers{ RHIBufferBarrier{ rhiStagingBuffer, RHIBarrierSync::Copy, RHIBarrierAccess::CopySource },
                         RHIBufferBarrier{ rhiDestBuffer, RHIBarrierSync::Copy, RHIBarrierAccess::CopyDest } };
    cmdList->Barrier(barriers, {});

    // Schedule a copy from the staging buffer to the destination texture.
    cmdList->Copy(rhiStagingBuffer,
                  rhiDestBuffer,
                  BufferCopyDescription{
                      .srcOffset = 0,
                      .dstOffset = actualSubresource.offset,
                      .size = actualSubresource.size,
                  });

    // Schedule a cleanup of the staging buffer.
    temporaryResources.emplace_back(std::move(rhiStagingBuffer));
}

void CommandContext::EnqueueDataUpload(const Texture& texture,
                                       std::span<const byte> data,
                                       std::span<const TextureUploadRegion> uploadRegions)
{
    // Validate that the upload regions match the raw data passed in.
    u64 packedDataByteSize = TextureUtil::ComputePackedUploadDataByteSize(texture.description, uploadRegions);
    VEX_CHECK(data.size_bytes() == packedDataByteSize,
              "Cannot enqueue a data upload: The passed in packed data's size ({}) must be equal to the total texture "
              "size computed from your specified upload regions ({}).",
              data.size_bytes(),
              packedDataByteSize);

    // Create aligned staging buffer.
    u64 stagingBufferByteSize = TextureUtil::ComputeAlignedUploadBufferByteSize(texture.description, uploadRegions);

    const BufferDescription stagingBufferDesc =
        CommandContext_Internal::GetStagingBufferDescription(texture.description.name, stagingBufferByteSize);

    Buffer stagingBuffer = backend->CreateBuffer(stagingBufferDesc, ResourceLifetime::Static);
    RHIBuffer& rhiStagingBuffer = backend->GetRHIBuffer(stagingBuffer.handle);
    RHITexture& rhiDestTexture = backend->GetRHITexture(texture.handle);

    // The staging buffer has to respect the alignment that which Vex uses for uploads.
    // We suppose however that user data is tightly packed.
    std::span<byte> stagingBufferData = rhiStagingBuffer.Map();
    CommandContext_Internal::UploadTextureDataAligned(texture.description, uploadRegions, data, stagingBufferData);
    rhiStagingBuffer.Unmap();

    RHIBufferBarrier stagingBufferBarrier{ rhiStagingBuffer, RHIBarrierSync::Copy, RHIBarrierAccess::CopySource };
    RHITextureBarrier textureBarrier{ rhiDestTexture,
                                      RHIBarrierSync::Copy,
                                      RHIBarrierAccess::CopyDest,
                                      RHITextureLayout::CopyDest };
    cmdList->Barrier({ &stagingBufferBarrier, 1 }, { &textureBarrier, 1 });

    const bool isFullUpload = uploadRegions.empty();
    if (isFullUpload)
    {
        // Perform a full copy, using the entire resource.
        cmdList->Copy(rhiStagingBuffer, rhiDestTexture);
    }
    else
    {
        // Otherwise we have to translate the TextureUploadRegions to their equivalent BufferToTextureCopyDescriptions.
        std::vector<BufferToTextureCopyDescription> copyDescriptions;
        copyDescriptions.reserve(uploadRegions.size());

        const float bytesPerPixel = TextureUtil::GetPixelByteSizeFromFormat(texture.description.format);
        u64 stagingBufferOffset = 0;

        for (const TextureUploadRegion& region : uploadRegions)
        {
            const u32 mipWidth = std::max(1u, texture.description.width >> region.mip);
            const u32 mipHeight = std::max(1u, texture.description.height >> region.mip);
            const u32 mipDepth = std::max(1u, texture.description.GetDepth() >> region.mip);

            // Use region extent if specified, otherwise use full mip dimensions.
            const u32 regionWidth = (region.extent.width == 0) ? mipWidth : region.extent.width;
            const u32 regionHeight = (region.extent.height == 0) ? mipHeight : region.extent.height;
            const u32 regionDepth = (region.extent.depth == 0) ? mipDepth : region.extent.depth;

            // Calculate the size of this region in the staging buffer.
            const u32 alignedRowPitch = AlignUp<u32>(regionWidth * bytesPerPixel, TextureUtil::RowPitchAlignment);
            const u64 regionStagingSize = static_cast<u64>(alignedRowPitch) * regionHeight * regionDepth;

            BufferToTextureCopyDescription copyDesc{
                .srcSubresource = { .offset = stagingBufferOffset, .size = regionStagingSize, },
                .dstSubresource = { .mip = region.mip,
                                    .startSlice = region.slice,
                                    .sliceCount = 1,
                                    .offset = region.offset, },
                .extent = { .width = regionWidth, .height = regionHeight, .depth = regionDepth, }
            };

            // Validate the translated description.
            TextureCopyUtil::ValidateBufferToTextureCopyDescription(stagingBufferDesc, texture.description, copyDesc);

            copyDescriptions.push_back(std::move(copyDesc));

            // Move to next aligned position in staging buffer.
            stagingBufferOffset += AlignUp<u64>(regionStagingSize, TextureUtil::MipAlignment);
        }
        cmdList->Copy(rhiStagingBuffer, rhiDestTexture, copyDescriptions);
    }

    // Schedule a cleanup of the staging buffer.
    temporaryResources.emplace_back(std::move(rhiStagingBuffer));
}

void CommandContext::EnqueueDataReadback(const Buffer& buffer, std::span<byte> output)
{
    // TODO(https://trello.com/c/WLWD8hyA): implement buffer readback
    VEX_NOT_YET_IMPLEMENTED();
}

void CommandContext::EnqueueDataReadback(const Texture& texture, std::span<byte> output)
{
    // TODO(https://trello.com/c/WLWD8hyA): implement texture readback
    VEX_NOT_YET_IMPLEMENTED();
}

BindlessHandle CommandContext::GetBindlessHandle(const ResourceBinding& resourceBinding)
{
    return GetBindlessHandles({ &resourceBinding, 1 })[0];
}

std::vector<BindlessHandle> CommandContext::GetBindlessHandles(std::span<const ResourceBinding> resourceBindings)
{
    std::vector<BindlessHandle> handles;
    handles.reserve(resourceBindings.size());
    for (const auto& binding : resourceBindings)
    {
        std::visit(Visitor{ [&handles, backend = backend](const BufferBinding& bufferBinding)
                            { handles.emplace_back(backend->GetBufferBindlessHandle(bufferBinding)); },
                            [&handles, backend = backend](const TextureBinding& texBinding)
                            { handles.emplace_back(backend->GetTextureBindlessHandle(texBinding)); } },
                   binding.binding);
    }
    return handles;
}

void CommandContext::TransitionBindings(std::span<const ResourceBinding> resourceBindings)
{
    // Collect all underlying RHI textures.
    std::vector<RHITextureBinding> rhiTextureBindings;
    rhiTextureBindings.reserve(resourceBindings.size());
    std::vector<RHIBufferBinding> rhiBufferBindings;
    rhiBufferBindings.reserve(resourceBindings.size());
    ResourceBindingUtils::CollectRHIResources(*backend, resourceBindings, rhiTextureBindings, rhiBufferBindings);

    // This code will be greatly simplified when we add caching of transitions until the next GPU operation.
    // See: https://trello.com/c/kJWhd2iu
    auto bufferBarriers = CommandContext_Internal::CreateBarriersFromBindings(rhiBufferBindings);
    auto textureBarriers = CommandContext_Internal::CreateBarriersFromBindings(rhiTextureBindings);
    cmdList->Barrier(bufferBarriers, textureBarriers);
}

void CommandContext::Barrier(const Texture& texture,
                             RHIBarrierSync newSync,
                             RHIBarrierAccess newAccess,
                             RHITextureLayout newLayout)
{
    cmdList->TextureBarrier(backend->GetRHITexture(texture.handle), newSync, newAccess, newLayout);
}

void CommandContext::Barrier(const Buffer& buffer, RHIBarrierSync newSync, RHIBarrierAccess newAccess)
{
    cmdList->BufferBarrier(backend->GetRHIBuffer(buffer.handle), newSync, newAccess);
}

std::vector<SyncToken> CommandContext::Submit()
{
    if (submissionPolicy != SubmissionPolicy::Immediate)
    {
        VEX_LOG(Fatal,
                "Cannot call submit when your submission policy is anything other than SubmissionPolicy::Immediate.");
    }
    hasSubmitted = true;
    return backend->EndCommandContext(*this);
}

void CommandContext::ExecuteInDrawContext(std::span<const TextureBinding> renderTargets,
                                          std::optional<const TextureBinding> depthStencil,
                                          const std::function<void()>& callback)
{
    std::vector<RHITextureBarrier> barriers;
    RHIDrawResources drawResources =
        ResourceBindingUtils::CollectRHIDrawResourcesAndBarriers(*backend, renderTargets, depthStencil, barriers);

    cmdList->Barrier({}, barriers);
    cmdList->BeginRendering(drawResources);
    callback();
    cmdList->EndRendering();
}

RHICommandList& CommandContext::GetRHICommandList()
{
    return *cmdList;
}

std::optional<RHIDrawResources> CommandContext::PrepareDrawCall(const DrawDescription& drawDesc,
                                                                const DrawResourceBinding& drawBindings,
                                                                std::optional<ConstantBinding> constants)
{
    VEX_CHECK(!drawBindings.depthStencil ||
                  (drawBindings.depthStencil &&
                   FormatIsDepthStencilCompatible(drawBindings.depthStencil->texture.description.format)),
              "The provided depth stencil should have a depth stencil format");
    VEX_CHECK(drawDesc.vertexShader.type == ShaderType::VertexShader,
              "Invalid type passed to Draw call for vertex shader: {}",
              drawDesc.vertexShader.type);
    VEX_CHECK(drawDesc.pixelShader.type == ShaderType::PixelShader,
              "Invalid type passed to Draw call for pixel shader: {}",
              drawDesc.pixelShader.type);

    // Transition RTs/DepthStencil
    std::vector<RHITextureBarrier> barriers;
    RHIDrawResources drawResources =
        ResourceBindingUtils::CollectRHIDrawResourcesAndBarriers(*backend,
                                                                 drawBindings.renderTargets,
                                                                 drawBindings.depthStencil,
                                                                 barriers);
    cmdList->Barrier({}, barriers);

    auto graphicsPSOKey = CommandContext_Internal::GetGraphicsPSOKeyFromDrawDesc(drawDesc, drawResources);

    if (!cachedGraphicsPSOKey || graphicsPSOKey != *cachedGraphicsPSOKey)
    {
        const RHIGraphicsPipelineState* pipelineState = backend->psCache.GetGraphicsPipelineState(graphicsPSOKey);
        // No valid PSO means we cannot proceed.
        if (!pipelineState)
        {
            return std::nullopt;
        }

        cmdList->SetPipelineState(*pipelineState);
        cachedGraphicsPSOKey = graphicsPSOKey;
    }

    // Setup the layout for our pass.
    RHIResourceLayout& resourceLayout = backend->psCache.GetResourceLayout();
    resourceLayout.SetLayoutResources(constants);

    cmdList->SetLayout(resourceLayout);

    if (!cachedInputAssembly || drawDesc.inputAssembly != cachedInputAssembly)
    {
        cmdList->SetInputAssembly(drawDesc.inputAssembly);
        cachedInputAssembly = drawDesc.inputAssembly;
    }

    // Transition and bind Vertex Buffer(s)
    SetVertexBuffers(drawBindings.vertexBuffersFirstSlot, drawBindings.vertexBuffers);

    // Transition and bind Index Buffer.
    SetIndexBuffer(drawBindings.indexBuffer);

    return drawResources;
}

void CommandContext::SetVertexBuffers(u32 vertexBuffersFirstSlot, std::span<BufferBinding> vertexBuffers)
{
    if (vertexBuffers.empty())
    {
        return;
    }

    std::vector<RHIBufferBarrier> barriers;
    std::vector<RHIBufferBinding> rhiBindings;
    barriers.reserve(vertexBuffers.size());
    rhiBindings.reserve(vertexBuffers.size());
    for (const auto& vertexBuffer : vertexBuffers)
    {
        if (!vertexBuffer.strideByteSize.has_value())
        {
            VEX_LOG(Fatal, "A vertex buffer must have a valid strideByteSize!");
        }
        RHIBuffer& buffer = backend->GetRHIBuffer(vertexBuffer.buffer.handle);
        rhiBindings.emplace_back(vertexBuffer, NonNullPtr(buffer));
        barriers.push_back(RHIBufferBarrier{ buffer, RHIBarrierSync::VertexInput, RHIBarrierAccess::VertexInputRead });
    }
    cmdList->Barrier(barriers, {});
    cmdList->SetVertexBuffers(vertexBuffersFirstSlot, rhiBindings);
}

void CommandContext::SetIndexBuffer(std::optional<BufferBinding> indexBuffer)
{
    if (!indexBuffer.has_value())
    {
        return;
    }

    RHIBuffer& buffer = backend->GetRHIBuffer(indexBuffer->buffer.handle);

    cmdList->BufferBarrier(buffer, RHIBarrierSync::VertexInput, RHIBarrierAccess::VertexInputRead);

    RHIBufferBinding binding{ *indexBuffer, NonNullPtr(buffer) };
    cmdList->SetIndexBuffer(binding);
}

} // namespace vex