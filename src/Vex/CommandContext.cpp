#include "CommandContext.h"

#include <algorithm>
#include <cmath>
#include <variant>

#include <Vex/ByteUtils.h>
#include <Vex/Containers/Utils.h>
#include <Vex/Debug.h>
#include <Vex/DrawHelpers.h>
#include <Vex/Graphics.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/PhysicalDevice.h>
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

static std::vector<BufferTextureCopyDesc> GetBufferTextureCopyDescFromTextureRegions(
    const TextureDesc& desc, std::span<const TextureRegion> regions)
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
                        .startSlice = region.subresource.startSlice,
                        .sliceCount = region.subresource.GetSliceCount(desc),
                    },
                    .offset = region.offset,
                    .extent = region.extent,
                },
            };

            copyDescs.push_back(std::move(copyDesc));

            // Move to next aligned position in staging buffer.
            stagingBufferOffset += AlignUp<u64>(regionStagingSize, TextureUtil::MipAlignment);
        }
    }

    return copyDescs;
}

static std::vector<RHIBufferBarrier> CreateBarriersFromBindings(RHIBarrierSync dstSync,
                                                                const std::vector<RHIBufferBinding>& rhiBufferBindings)
{
    std::vector<RHIBufferBarrier> barriers;
    barriers.reserve(rhiBufferBindings.size());
    for (const auto& rhiBinding : rhiBufferBindings)
    {
        barriers.push_back(ResourceBindingUtils::CreateBarrierFromRHIBinding(dstSync, rhiBinding));
    }
    return barriers;
}

static std::vector<RHITextureBarrier> CreateBarriersFromBindings(
    RHIBarrierSync dstSync, const std::vector<RHITextureBinding>& rhiTextureBindings)
{
    std::vector<RHITextureBarrier> barriers;
    barriers.reserve(rhiTextureBindings.size());
    for (const auto& rhiBinding : rhiTextureBindings)
    {
        barriers.push_back(ResourceBindingUtils::CreateBarrierFromRHIBinding(dstSync, rhiBinding));
    }
    return barriers;
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

TextureReadbackContext::TextureReadbackContext(const Buffer& buffer,
                                               std::span<const TextureRegion> textureRegions,
                                               const TextureDesc& desc,
                                               Graphics& backend)
    : buffer{ buffer }
    , textureRegions{ textureRegions.begin(), textureRegions.end() }
    , textureDesc{ desc }
    , backend{ backend }
{
}

void BufferReadbackContext::ReadData(std::span<byte> outData)
{
    RHIBuffer& rhiBuffer = backend->GetRHIBuffer(buffer.handle);

    std::span<byte> bufferData = rhiBuffer.Map();
    std::copy(bufferData.begin(), bufferData.begin() + outData.size(), outData.begin());
    rhiBuffer.Unmap();
}

u64 BufferReadbackContext::GetDataByteSize() const noexcept
{
    return buffer.desc.byteSize;
}

BufferReadbackContext::~BufferReadbackContext()
{
    backend->DestroyBuffer(buffer);
}

BufferReadbackContext::BufferReadbackContext(BufferReadbackContext&& other)
    : buffer(std::exchange(other.buffer, {}))
    , backend{ other.backend }
{
}

BufferReadbackContext& BufferReadbackContext::operator=(BufferReadbackContext&& other)
{
    if (this != &other)
    {
        std::swap(buffer, other.buffer);
        backend = other.backend;
    }
    return *this;
}

BufferReadbackContext::BufferReadbackContext(const Buffer& buffer, Graphics& backend)
    : buffer{ buffer }
    , backend{ backend }
{
}

TextureReadbackContext::~TextureReadbackContext()
{
    if (buffer.handle != GInvalidBufferHandle)
    {
        backend->DestroyBuffer(buffer);
    }
}

TextureReadbackContext::TextureReadbackContext(TextureReadbackContext&& other)
    : buffer(std::exchange(other.buffer, {}))
    , textureRegions{ std::move(other.textureRegions) }
    , textureDesc{ std::move(other.textureDesc) }
    , backend{ other.backend }
{
}

TextureReadbackContext& TextureReadbackContext::operator=(TextureReadbackContext&& other)
{
    if (this != &other)
    {
        std::swap(buffer, other.buffer);
        std::swap(textureRegions, other.textureRegions);
        std::swap(textureDesc, other.textureDesc);
        backend = other.backend;
    }
    return *this;
}

void TextureReadbackContext::ReadData(std::span<byte> outData) const
{
    RHIBuffer& rhiBuffer = backend->GetRHIBuffer(buffer.handle);

    std::span<byte> bufferData = rhiBuffer.Map();
    TextureCopyUtil::ReadTextureDataAligned(textureDesc, textureRegions, bufferData, outData);
    rhiBuffer.Unmap();
}

u64 TextureReadbackContext::GetDataByteSize() const noexcept
{
    return TextureUtil::ComputePackedTextureDataByteSize(textureDesc, textureRegions);
}

CommandContext::CommandContext(NonNullPtr<Graphics> backend,
                               NonNullPtr<RHICommandList> cmdList,
                               NonNullPtr<RHITimestampQueryPool> queryPool,
                               SubmissionPolicy submissionPolicy,
                               std::span<SyncToken> dependencies)
    : backend(backend)
    , cmdList(cmdList)
    , submissionPolicy(submissionPolicy)
    , dependencies{ dependencies.begin(), dependencies.end() }
{
    cmdList->Open();
    cmdList->SetTimestampQueryPool(queryPool);
    if (cmdList->GetType() != QueueType::Copy)
    {
        cmdList->SetDescriptorPool(*backend->descriptorPool, backend->psCache->GetResourceLayout());
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
    if (!(binding.texture.desc.usage & (TextureUsage::RenderTarget | TextureUsage::DepthStencil)))
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
                          static_cast<TextureUsage::Type>(binding.texture.desc.usage &
                                                          (TextureUsage::RenderTarget | TextureUsage::DepthStencil)),
                          textureClearValue.value_or(binding.texture.desc.clearValue));
}

void CommandContext::Draw(const DrawDesc& drawDesc,
                          const DrawResourceBinding& drawBindings,
                          ConstantBinding constants,
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

void CommandContext::DrawIndexed(const DrawDesc& drawDesc,
                                 const DrawResourceBinding& drawBindings,
                                 ConstantBinding constants,
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

void CommandContext::Dispatch(const ShaderKey& shader, ConstantBinding constants, std::array<u32, 3> groupCount)
{
    if (shader.type != ShaderType::ComputeShader)
    {
        VEX_LOG(Fatal, "Invalid shader type passed to Dispatch call: {}", shader.type);
    }

    using namespace CommandContext_Internal;

    ComputePipelineStateKey psoKey = { .computeShader = shader };
    if (!cachedComputePSOKey || psoKey != cachedComputePSOKey)
    {
        // Register shader and get Pipeline if exists (if not create it).
        const RHIComputePipelineState* pipelineState = backend->psCache->GetComputePipelineState(psoKey);

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
    RHIResourceLayout& resourceLayout = backend->psCache->GetResourceLayout();
    resourceLayout.SetLayoutResources(constants);
    cmdList->SetLayout(resourceLayout);

    // Validate dispatch (vs platform/api constraints)
    // backend->ValidateDispatch(groupCount);

    // Perform dispatch
    cmdList->Dispatch(groupCount);
}

void CommandContext::TraceRays(const RayTracingPassDescription& rayTracingPassDescription,
                               ConstantBinding constants,
                               std::array<u32, 3> widthHeightDepth)
{
    RayTracingPassDescription::ValidateShaderTypes(rayTracingPassDescription);

    const RHIRayTracingPipelineState* pipelineState =
        backend->psCache->GetRayTracingPipelineState(rayTracingPassDescription, *backend->allocator);
    if (!pipelineState)
    {
        VEX_LOG(Error, "PSO cache returned an invalid pipeline state, unable to continue dispatch...");
        return;
    }
    cmdList->SetPipelineState(*pipelineState);

    // Sets the resource layout to use for the ray trace.
    RHIResourceLayout& resourceLayout = backend->psCache->GetResourceLayout();
    resourceLayout.SetLayoutResources(constants);

    cmdList->SetLayout(resourceLayout);

    // Validate ray trace (vs platform/api constraints)
    // backend->ValidateTraceRays(widthHeightDepth);

    cmdList->TraceRays(widthHeightDepth, *pipelineState);
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
        GPhysicalDevice->featureChecker->FormatSupportsLinearFiltering(texture.desc.format, textureBinding.isSRGB);
    const bool textureFormatSupportsMipGeneration = FormatUtil::SupportsMipGeneration(texture.desc.format);
    VEX_CHECK(textureFormatSupportsMipGeneration && apiFormatSupportsLinearFiltering,
              "The texture's format must be a valid format for mip generation. Only uncompressed floating point / "
              "normalized color formats are supported.");

    VEX_CHECK(cmdList->GetType() != QueueType::Copy,
              "Mip Generation requires a Compute or Graphics command list type.");

    // Built-in mip generation is leveraged if supported (and if we're using a graphics command queue).
    // If we want to perform SRGB mip generation, we must do it manually.
    if (GPhysicalDevice->featureChecker->IsFeatureSupported(Feature::MipGeneration) &&
        cmdList->GetType() == QueueType::Graphics && !textureBinding.isSRGB)
    {
        cmdList->GenerateMips(backend->GetRHITexture(texture.handle), textureBinding.subresource);
        return;
    }

    static constexpr std::string_view MipGenerationEntryPoint = "MipGenerationCS";

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

    // We have to perform a manual mip generation if not supported by the graphics API.
    ShaderKey mipGenerationShaderKey{
        .path = std::filesystem::current_path() / "MipGeneration.hlsl",
        .entryPoint = std::string(MipGenerationEntryPoint),
        .type = ShaderType::ComputeShader,
        .defines = { ShaderDefine{ "TEXTURE_TYPE", std::string(FormatUtil::GetHLSLType(texture.desc.format)) },
                     ShaderDefine{ "TEXTURE_DIMENSION", GetTextureDimensionDefine(texture.desc.type) },
                     ShaderDefine{ "LINEAR_SAMPLER_SLOT", std::format("s{}", backend->builtInLinearSamplerSlot) },
                     ShaderDefine{ "CONVERT_TO_SRGB", textureBinding.isSRGB ? "1" : "0" },
                     ShaderDefine{ "NON_POWER_OF_TWO" } }
    };
    const u32 nonPowerOfTwoDefineIndex = mipGenerationShaderKey.defines.size() - 1;

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

        mipGenerationShaderKey.defines[nonPowerOfTwoDefineIndex].value =
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
        auto handles = GetBindlessHandles(bindings);
        TransitionBindings(bindings);

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
        Dispatch(mipGenerationShaderKey, ConstantBinding(uniforms), dispatchGroupCount);

        width = std::max(1u, width >> (1 + !isLastIteration));
        height = std::max(1u, height >> (1 + !isLastIteration));
        depth = std::max(1u, depth >> (1 + !isLastIteration));

        mip += 1 + !isLastIteration;
    }

    // Transfers the entirety of the resource back to ShaderRead, ready for use in a shader.
    TextureBinding finalBinding{
        .texture = texture,
        .usage = TextureBindingUsage::ShaderRead,
    };
    TransitionBindings(std::span<const ResourceBinding>{ { finalBinding } });
}

void CommandContext::Copy(const Texture& source, const Texture& destination)
{
    VEX_CHECK(source.handle != destination.handle, "Cannot copy a texture to itself!");

    TextureUtil::ValidateCompatibleTextureDescs(source.desc, destination.desc);

    RHITexture& sourceRHI = backend->GetRHITexture(source.handle);
    RHITexture& destinationRHI = backend->GetRHITexture(destination.handle);
    std::array barriers{ RHITextureBarrier{ sourceRHI,
                                            TextureSubresource{},
                                            RHIBarrierSync::Copy,
                                            RHIBarrierAccess::CopySource,
                                            RHITextureLayout::CopySource },
                         RHITextureBarrier{ destinationRHI,
                                            TextureSubresource{},
                                            RHIBarrierSync::Copy,
                                            RHIBarrierAccess::CopyDest,
                                            RHITextureLayout::CopyDest } };
    cmdList->Barrier({}, barriers);
    cmdList->Copy(sourceRHI, destinationRHI);
}

void CommandContext::Copy(const Texture& source, const Texture& destination, const TextureCopyDesc& regionMapping)
{
    Copy(source, destination, { &regionMapping, 1 });
}

void CommandContext::Copy(const Texture& source,
                          const Texture& destination,
                          std::span<const TextureCopyDesc> regionMappings)
{
    VEX_CHECK(source.handle != destination.handle, "Cannot copy a texture to itself!");

    for (auto& mapping : regionMappings)
    {
        TextureUtil::ValidateCopyDesc(source.desc, destination.desc, mapping);
    }

    RHITexture& sourceRHI = backend->GetRHITexture(source.handle);
    RHITexture& destinationRHI = backend->GetRHITexture(destination.handle);
    std::array barriers{ RHITextureBarrier{ sourceRHI,
                                            TextureSubresource{},
                                            RHIBarrierSync::Copy,
                                            RHIBarrierAccess::CopySource,
                                            RHITextureLayout::CopySource },
                         RHITextureBarrier{ destinationRHI,
                                            TextureSubresource{},
                                            RHIBarrierSync::Copy,
                                            RHIBarrierAccess::CopyDest,
                                            RHITextureLayout::CopyDest } };
    cmdList->Barrier({}, barriers);
    cmdList->Copy(sourceRHI, destinationRHI, regionMappings);
}

void CommandContext::Copy(const Buffer& source, const Buffer& destination)
{
    VEX_CHECK(source.handle != destination.handle, "Cannot copy a buffer to itself!");

    BufferUtil::ValidateSimpleBufferCopy(source.desc, destination.desc);

    RHIBuffer& sourceRHI = backend->GetRHIBuffer(source.handle);
    RHIBuffer& destinationRHI = backend->GetRHIBuffer(destination.handle);
    std::array barriers{ RHIBufferBarrier{ sourceRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopySource },
                         RHIBufferBarrier{ destinationRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopyDest } };
    cmdList->Barrier(barriers, {});
    cmdList->Copy(sourceRHI, destinationRHI);
}

void CommandContext::Copy(const Buffer& source, const Buffer& destination, const BufferCopyDesc& bufferCopyDesc)
{
    VEX_CHECK(source.handle != destination.handle, "Cannot copy a buffer to itself!");

    BufferUtil::ValidateBufferCopyDesc(source.desc, destination.desc, bufferCopyDesc);

    RHIBuffer& sourceRHI = backend->GetRHIBuffer(source.handle);
    RHIBuffer& destinationRHI = backend->GetRHIBuffer(destination.handle);
    std::array barriers{ RHIBufferBarrier{ sourceRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopySource },
                         RHIBufferBarrier{ destinationRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopyDest } };
    cmdList->Barrier(barriers, {});

    cmdList->Copy(sourceRHI, destinationRHI, bufferCopyDesc);
}

void CommandContext::Copy(const Buffer& source, const Texture& destination)
{
    RHIBuffer& sourceRHI = backend->GetRHIBuffer(source.handle);
    RHITexture& destinationRHI = backend->GetRHITexture(destination.handle);
    RHIBufferBarrier sourceBarrier{ sourceRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopySource };
    RHITextureBarrier destinationBarrier{ destinationRHI,
                                          TextureSubresource{},
                                          RHIBarrierSync::Copy,
                                          RHIBarrierAccess::CopyDest,
                                          RHITextureLayout::CopyDest };
    cmdList->Barrier({ &sourceBarrier, 1 }, { &destinationBarrier, 1 });
    cmdList->Copy(sourceRHI, destinationRHI);
}

void CommandContext::Copy(const Buffer& source, const Texture& destination, const BufferTextureCopyDesc& copyDesc)
{
    Copy(source, destination, { &copyDesc, 1 });
}

void CommandContext::Copy(const Buffer& source,
                          const Texture& destination,
                          std::span<const BufferTextureCopyDesc> copyDescs)
{
    for (auto& copyDesc : copyDescs)
    {
        TextureCopyUtil::ValidateBufferTextureCopyDesc(source.desc, destination.desc, copyDesc);
    }

    RHIBuffer& sourceRHI = backend->GetRHIBuffer(source.handle);
    RHITexture& destinationRHI = backend->GetRHITexture(destination.handle);
    RHIBufferBarrier sourceBarrier{ sourceRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopySource };
    RHITextureBarrier destinationBarrier{ destinationRHI,
                                          TextureSubresource{},
                                          RHIBarrierSync::Copy,
                                          RHIBarrierAccess::CopyDest,
                                          RHITextureLayout::CopyDest };
    cmdList->Barrier({ &sourceBarrier, 1 }, { &destinationBarrier, 1 });
    cmdList->Copy(sourceRHI, destinationRHI, copyDescs);
}

void CommandContext::Copy(const Texture& source, const Buffer& destination)
{
    Copy(source, destination, BufferTextureCopyDesc::AllMips(source.desc));
}

void CommandContext::Copy(const Texture& source,
                          const Buffer& destination,
                          std::span<const BufferTextureCopyDesc> bufferToTextureCopyDescriptions)
{
    for (auto& copyDesc : bufferToTextureCopyDescriptions)
    {
        TextureCopyUtil::ValidateBufferTextureCopyDesc(destination.desc, source.desc, copyDesc);
    }

    RHITexture& sourceRHI = backend->GetRHITexture(source.handle);
    RHIBuffer& destinationRHI = backend->GetRHIBuffer(destination.handle);
    RHITextureBarrier sourceBarrier{ sourceRHI,
                                     {},
                                     RHIBarrierSync::Copy,
                                     RHIBarrierAccess::CopySource,
                                     RHITextureLayout::CopySource };
    RHIBufferBarrier destinationBarrier{ destinationRHI, RHIBarrierSync::Copy, RHIBarrierAccess::CopyDest };
    cmdList->Barrier({ &destinationBarrier, 1 }, { &sourceBarrier, 1 });

    if (FormatUtil::SupportsStencil(source.desc.format) &&
        !GPhysicalDevice->featureChecker->IsFeatureSupported(Feature::DepthStencilReadback))
    {
        // Run compute to copy the image to the buffer
        // See: https://trello.com/c/vEaa2SUe
        VEX_NOT_YET_IMPLEMENTED();
    }
    else
    {
        cmdList->Copy(sourceRHI, destinationRHI, bufferToTextureCopyDescriptions);
    }
}

void CommandContext::EnqueueDataUpload(const Buffer& buffer, std::span<const byte> data, const BufferRegion& region)
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
        RHIBuffer& rhiDestBuffer = backend->GetRHIBuffer(buffer.handle);
        ResourceMappedMemory(rhiDestBuffer).WriteData(data, region.offset);
        return;
    }

    BufferUtil::ValidateBufferRegion(buffer.desc, region);

    // Buffer creation invalidates pointers to existing RHI buffers.
    Buffer stagingBuffer = backend->CreateBuffer(
        BufferDesc::CreateStagingBufferDesc(buffer.desc.name + "_staging", region.GetByteSize(buffer.desc)));

    RHIBuffer& rhiStagingBuffer = backend->GetRHIBuffer(stagingBuffer.handle);
    ResourceMappedMemory(rhiStagingBuffer).WriteData(data);

    Copy(stagingBuffer,
         buffer,
         BufferCopyDesc{
             .srcOffset = 0,
             .dstOffset = region.offset,
             .byteSize = region.GetByteSize(buffer.desc),
         });

    // Schedule a cleanup of the staging buffer.
    temporaryResources.emplace_back(stagingBuffer);
}

void CommandContext::EnqueueDataUpload(const Texture& texture,
                                       std::span<const byte> packedData,
                                       std::span<const TextureRegion> textureRegions)
{
    // Validate that the upload regions match the raw data passed in.
    u64 packedDataByteSize = TextureUtil::ComputePackedTextureDataByteSize(texture.desc, textureRegions);
    VEX_CHECK(packedData.size_bytes() == packedDataByteSize,
              "Cannot enqueue a data upload: The passed in packed data's size ({}) must be equal to the total texture "
              "size computed from your specified upload regions ({}).",
              packedData.size_bytes(),
              packedDataByteSize);

    // Create aligned staging buffer.
    u64 stagingBufferByteSize = TextureUtil::ComputeAlignedUploadBufferByteSize(texture.desc, textureRegions);

    const BufferDesc stagingBufferDesc =
        BufferDesc::CreateStagingBufferDesc(texture.desc.name + "_staging", stagingBufferByteSize);

    Buffer stagingBuffer = backend->CreateBuffer(stagingBufferDesc);
    RHIBuffer& rhiStagingBuffer = backend->GetRHIBuffer(stagingBuffer.handle);

    // The staging buffer has to respect the alignment that which Vex uses for uploads.
    // We suppose however that user data is tightly packed.
    std::span<byte> stagingBufferData = rhiStagingBuffer.Map();
    TextureCopyUtil::WriteTextureDataAligned(texture.desc, textureRegions, packedData, stagingBufferData);
    rhiStagingBuffer.Unmap();

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

    // Schedule a cleanup of the staging buffer.
    temporaryResources.emplace_back(stagingBuffer);
}

void CommandContext::EnqueueDataUpload(const Texture& texture,
                                       std::span<const byte> packedData,
                                       const TextureRegion& textureRegion)
{
    EnqueueDataUpload(texture, packedData, { &textureRegion, 1 });
}

TextureReadbackContext CommandContext::EnqueueDataReadback(const Texture& srcTexture,
                                                           std::span<const TextureRegion> textureRegions)
{
    // Create packed readback buffer.
    u64 stagingBufferByteSize = TextureUtil::ComputeAlignedUploadBufferByteSize(srcTexture.desc, textureRegions);
    const BufferDesc readbackBufferDesc =
        BufferDesc::CreateReadbackBufferDesc(srcTexture.desc.name + "_readback", stagingBufferByteSize);

    Buffer stagingBuffer = backend->CreateBuffer(readbackBufferDesc, ResourceLifetime::Static);

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

    return { stagingBuffer, textureRegions, srcTexture.desc, *backend };
}

TextureReadbackContext CommandContext::EnqueueDataReadback(const Texture& srcTexture,
                                                           const TextureRegion& textureRegion)
{
    return EnqueueDataReadback(srcTexture, { &textureRegion, 1 });
}

BufferReadbackContext CommandContext::EnqueueDataReadback(const Buffer& srcBuffer)
{
    // Create packed readback buffer.
    const BufferDesc readbackBufferDesc =
        BufferDesc::CreateReadbackBufferDesc(srcBuffer.desc.name + "_readback", srcBuffer.desc.byteSize);
    Buffer stagingBuffer = backend->CreateBuffer(readbackBufferDesc, ResourceLifetime::Static);

    Copy(srcBuffer, stagingBuffer);

    return { stagingBuffer, *backend };
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

    const RHIBarrierSync dstSync =
        cmdList->GetType() == QueueTypes::Compute ? RHIBarrierSync::ComputeShader : RHIBarrierSync::AllGraphics;

    auto bufferBarriers = CommandContext_Internal::CreateBarriersFromBindings(dstSync, rhiBufferBindings);
    auto textureBarriers = CommandContext_Internal::CreateBarriersFromBindings(dstSync, rhiTextureBindings);
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

SyncToken CommandContext::Submit()
{
    if (submissionPolicy != SubmissionPolicy::Immediate)
    {
        VEX_LOG(Fatal,
                "Cannot call submit when your submission policy is anything other than SubmissionPolicy::Immediate.");
    }
    hasSubmitted = true;

    std::vector<SyncToken> tokens = backend->EndCommandContext(*this);
    VEX_ASSERT(tokens.size() == 1);
    return tokens[0];
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

QueryHandle CommandContext::BeginTimestampQuery()
{
    return cmdList->BeginTimestampQuery();
}

void CommandContext::EndTimestampQuery(QueryHandle handle)
{
    cmdList->EndTimestampQuery(handle);
}

RHICommandList& CommandContext::GetRHICommandList()
{
    return *cmdList;
}

ScopedGPUEvent CommandContext::CreateScopedGPUEvent(const char* markerLabel, std::array<float, 3> color)
{
    return { cmdList->CreateScopedMarker(markerLabel, color) };
}

std::optional<RHIDrawResources> CommandContext::PrepareDrawCall(const DrawDesc& drawDesc,
                                                                const DrawResourceBinding& drawBindings,
                                                                ConstantBinding constants)
{
    VEX_CHECK(!drawBindings.depthStencil ||
                  (drawBindings.depthStencil &&
                   FormatUtil::IsDepthStencilCompatible(drawBindings.depthStencil->texture.desc.format)),
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
        const RHIGraphicsPipelineState* pipelineState = backend->psCache->GetGraphicsPipelineState(graphicsPSOKey);
        // No valid PSO means we cannot proceed.
        if (!pipelineState)
        {
            return std::nullopt;
        }

        cmdList->SetPipelineState(*pipelineState);
        cachedGraphicsPSOKey = graphicsPSOKey;
    }

    // Setup the layout for our pass.
    RHIResourceLayout& resourceLayout = backend->psCache->GetResourceLayout();
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