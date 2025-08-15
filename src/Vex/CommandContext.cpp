#include "CommandContext.h"

#include <Vex/Debug.h>
#include <Vex/DrawHelpers.h>
#include <Vex/GfxBackend.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/RHIBindings.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>
#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/RHIImpl/RHISwapChain.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/ResourceBindingSet.h>
#include <Vex/ShaderResourceContext.h>

namespace vex
{

namespace CommandContext_Internal
{

static GraphicsPipelineStateKey GetGraphicsPSOKeyFromDrawDesc(const DrawDescription& drawDesc,
                                                              std::span<const ResourceBinding> renderTargetBindings,
                                                              std::optional<ResourceBinding> depthStencilBinding)
{
    GraphicsPipelineStateKey key{ .vertexShader = drawDesc.vertexShader,
                                  .pixelShader = drawDesc.pixelShader,
                                  .vertexInputLayout = drawDesc.vertexInputLayout,
                                  .inputAssembly = drawDesc.inputAssembly,
                                  .rasterizerState = drawDesc.rasterizerState,
                                  .depthStencilState = drawDesc.depthStencilState,
                                  .colorBlendState = drawDesc.colorBlendState };

    for (const ResourceBinding& binding : renderTargetBindings)
    {
        key.renderTargetState.colorFormats.emplace_back(binding.texture.description.format);
    }

    key.renderTargetState.depthStencilFormat =
        depthStencilBinding.has_value() ? depthStencilBinding->texture.description.format : TextureFormat::UNKNOWN;

    // Ensure each rendertarget has atleast a default color attachment (no blending, write all).
    key.colorBlendState.attachments.resize(renderTargetBindings.size());

    return key;
}

static void ValidateDrawResources(const DrawResources& drawResources)
{
    ResourceBinding::ValidateResourceBindings(drawResources.readResources,
                                              TextureUsage::ShaderRead,
                                              BufferUsage::UniformBuffer | BufferUsage::GenericBuffer);
    ResourceBinding::ValidateResourceBindings(drawResources.unorderedAccessResources,
                                              TextureUsage::ShaderReadWrite,
                                              BufferUsage::ReadWriteBuffer);
    ResourceBinding::ValidateResourceBindings(drawResources.renderTargets, TextureUsage::RenderTarget);
    if (drawResources.depthStencil)
    {
        ResourceBinding::ValidateResourceBindings({ { *drawResources.depthStencil } }, TextureUsage::DepthStencil);
    }
}

} // namespace CommandContext_Internal

CommandContext::CommandContext(GfxBackend* backend, RHICommandList* cmdList)
    : backend(backend)
    , cmdList(cmdList)
{
    cmdList->Open();
    cmdList->SetDescriptorPool(*backend->descriptorPool, backend->GetPipelineStateCache().GetResourceLayout());
}

CommandContext::~CommandContext()
{
    backend->EndCommandContext(*cmdList);
}

void CommandContext::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
    cmdList->SetViewport(x, y, width, height, minDepth, maxDepth);
}

void CommandContext::SetScissor(i32 x, i32 y, u32 width, u32 height)
{
    cmdList->SetScissor(x, y, width, height);
}

void CommandContext::ClearTexture(ResourceBinding binding,
                                  TextureClearValue* optionalTextureClearValue,
                                  std::optional<std::array<float, 4>> clearRect)
{
    if (!binding.IsTexture())
    {
        VEX_LOG(Fatal, "ClearTexture can only take in a texture.");
    }
    RHITexture& texture = backend->GetRHITexture(binding.texture.handle);
    cmdList->Transition(texture, texture.GetClearTextureState());
    cmdList->ClearTexture(
        texture,
        binding,
        optionalTextureClearValue ? *optionalTextureClearValue : binding.texture.description.clearValue);
}

void CommandContext::Draw(const DrawDescription& drawDesc, const DrawResources& drawResources, u32 vertexCount)
{
    using namespace vex::CommandContext_Internal;

    ValidateDrawResources(drawResources);

    // Collect all underlying RHI textures.
    i32 totalSize =
        static_cast<i32>(drawResources.readResources.size() + drawResources.unorderedAccessResources.size() +
                         drawResources.renderTargets.size() + (drawResources.depthStencil.has_value() ? 1 : 0));

    // Conservative allocation.
    std::vector<RHITextureBinding> rhiTextureBindings;
    rhiTextureBindings.reserve(totalSize);
    std::vector<RHIBufferBinding> rhiBufferBindings;
    rhiBufferBindings.reserve(totalSize);

    std::vector<std::pair<RHITexture&, RHITextureState::Flags>> textureStateTransitions;
    textureStateTransitions.reserve(totalSize);
    std::vector<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferStateTransitions;
    bufferStateTransitions.reserve(totalSize);

    // Textures
    {
        auto CollectTextures = [backend = backend, &rhiTextureBindings, &textureStateTransitions](
                                   std::span<const ResourceBinding> resources,
                                   TextureUsage::Type usage,
                                   RHITextureState::Flags state)
        {
            for (const ResourceBinding& binding : resources)
            {
                if (!binding.IsTexture())
                {
                    continue;
                }
                auto& texture = backend->GetRHITexture(binding.texture.handle);
                textureStateTransitions.emplace_back(texture, state);
                rhiTextureBindings.emplace_back(binding, usage, &texture);
            }
        };
        CollectTextures(drawResources.readResources, TextureUsage::ShaderRead, RHITextureState::ShaderResource);
        CollectTextures(drawResources.unorderedAccessResources,
                        TextureUsage::ShaderReadWrite,
                        RHITextureState::ShaderReadWrite);
        CollectTextures(drawResources.renderTargets, TextureUsage::RenderTarget, RHITextureState::RenderTarget);
        if (drawResources.depthStencil)
        {
            CollectTextures({ { *drawResources.depthStencil } },
                            TextureUsage::DepthStencil,
                            RHITextureState::DepthWrite);
        }
    }

    // Buffers
    {
        auto CollectBuffers = [backend = backend, &rhiBufferBindings, &bufferStateTransitions](
                                  std::span<const ResourceBinding> resources,
                                  BufferUsage::Flags usage,
                                  RHIBufferState::Flags state)
        {
            for (const ResourceBinding& binding : resources)
            {
                if (!binding.IsBuffer())
                {
                    continue;
                }

                if (IsBindingUsageCompatibleWithBufferUsage(usage, binding.bufferUsage))
                {
                    auto& buffer = backend->GetRHIBuffer(binding.buffer.handle);
                    bufferStateTransitions.emplace_back(buffer, state);
                    rhiBufferBindings.emplace_back(binding, &buffer);
                }
            }
        };
        CollectBuffers(drawResources.readResources,
                       BufferUsage::UniformBuffer | BufferUsage::GenericBuffer,
                       RHIBufferState::ShaderResource);
        CollectBuffers(drawResources.unorderedAccessResources,
                       BufferUsage::ReadWriteBuffer,
                       RHIBufferState::ShaderReadWrite);
    }

    auto graphicsPSOKey =
        GetGraphicsPSOKeyFromDrawDesc(drawDesc, drawResources.renderTargets, drawResources.depthStencil);
    if (!cachedGraphicsPSOKey || graphicsPSOKey != *cachedGraphicsPSOKey)
    {
        auto pipelineState = backend->GetPipelineStateCache().GetGraphicsPipelineState(
            graphicsPSOKey,
            ShaderResourceContext{ rhiTextureBindings, rhiBufferBindings });
        // No valid PSO means we cannot proceed.
        if (!pipelineState)
        {
            return;
        }

        cmdList->SetPipelineState(*pipelineState);
        cachedGraphicsPSOKey = graphicsPSOKey;
    }

    // Transition our resources to the correct states.
    cmdList->Transition(textureStateTransitions);
    cmdList->Transition(bufferStateTransitions);

    // TODO(https://trello.com/c/IGxuLci9): Correctly integrate buffers here, including staging upload.
    // std::vector<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferStagingCopyTransitions;
    // for (u32 i = 0; i < rhiBufferBindings.size(); ++i)
    //{
    //     bufferStateTransitions.emplace_back(
    //         *rhiBufferBindings[i].buffer,
    //         i < bufferWriteCount ? RHIBufferState::ShaderReadWrite : RHIBufferState::ShaderResource);

    //    if (rhiBufferBindings[i].buffer->NeedsStagingBufferCopy())
    //    {
    //        bufferStagingCopyTransitions.emplace_back(*rhiBufferBindings[i].buffer, RHIBufferState::CopyDest);
    //        bufferStagingCopyTransitions.emplace_back(*rhiBufferBindings[i].buffer->GetStagingBuffer(),
    //                                                  RHIBufferState::CopySource);
    //    }
    //}

    // cmdList->Transition(bufferStagingCopyTransitions);
    // for (int i = 0; i < bufferStagingCopyTransitions.size() / 2; ++i)
    //{
    //     auto& buffer = bufferStagingCopyTransitions[i * 2].first;
    //     cmdList->Copy(*buffer.GetStagingBuffer(), buffer);
    //     buffer.SetNeedsStagingBufferCopy(false);
    // }

    // Setup the layout for our pass.
    RHIResourceLayout& resourceLayout = backend->GetPipelineStateCache().GetResourceLayout();
    resourceLayout.SetLayoutResources(backend->rhi,
                                      backend->resourceCleanup,
                                      drawResources.constants,
                                      rhiTextureBindings,
                                      rhiBufferBindings,
                                      *backend->descriptorPool);

    cmdList->SetLayout(resourceLayout, *backend->descriptorPool);

    if (!cachedInputAssembly || drawDesc.inputAssembly != cachedInputAssembly)
    {
        cmdList->SetInputAssembly(drawDesc.inputAssembly);
        cachedInputAssembly = drawDesc.inputAssembly;
    }

    // TODO(https://trello.com/c/IGxuLci9): Validate draw vertex count (eg: versus the currently used index buffer size)

    RHIDrawResources rhiDrawRes;

    std::ranges::copy_if(rhiTextureBindings,
                         std::back_inserter(rhiDrawRes.renderTargets),
                         [](const auto& rhiTexBinding) { return rhiTexBinding.usage == TextureUsage::RenderTarget; });

    if (auto it = std::ranges::find_if(rhiTextureBindings,
                                       [](const auto& rhiTexBinding)
                                       { return rhiTexBinding.usage == TextureUsage::DepthStencil; });
        it != rhiTextureBindings.end())
    {
        rhiDrawRes.depthStencil = *it;
    }

    cmdList->BeginRendering(rhiDrawRes);

    cmdList->Draw(vertexCount);

    cmdList->EndRendering();
}

void CommandContext::Dispatch(const ShaderKey& shader,
                              const ResourceBindingSet& resourceBindingSet,
                              std::array<u32, 3> groupCount)
{
    resourceBindingSet.ValidateBindings();

    // Collect all underlying RHI textures.
    std::vector<RHITextureBinding> rhiTextureBindings;
    std::vector<RHIBufferBinding> rhiBufferBindings;
    ResourceBindingSet::CollectRHIResources(*backend,
                                            resourceBindingSet.writes,
                                            rhiTextureBindings,
                                            rhiBufferBindings,
                                            TextureUsage::ShaderReadWrite,
                                            BufferUsage::ReadWriteBuffer);

    const u32 texWriteCount = rhiTextureBindings.size();
    const u32 bufferWriteCount = rhiBufferBindings.size();
    ResourceBindingSet::CollectRHIResources(*backend,
                                            resourceBindingSet.reads,
                                            rhiTextureBindings,
                                            rhiBufferBindings,
                                            TextureUsage::ShaderRead,
                                            BufferUsage::UniformBuffer | BufferUsage::GenericBuffer);

    // This code will be greatly simplified when we add caching of transitions until the next GPU operation.
    // See: https://trello.com/c/kJWhd2iu

    std::vector<std::pair<RHITexture&, RHITextureState::Flags>> textureStateTransitions;
    textureStateTransitions.reserve(rhiTextureBindings.size());

    for (u32 i = 0; i < rhiTextureBindings.size(); ++i)
    {
        textureStateTransitions.emplace_back(
            *rhiTextureBindings[i].texture,
            i < texWriteCount ? RHITextureState::ShaderReadWrite : RHITextureState::ShaderResource);
    }

    std::vector<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferStateTransitions;
    bufferStateTransitions.reserve(rhiBufferBindings.size());

    std::vector<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferStagingCopyTransitions;
    for (u32 i = 0; i < rhiBufferBindings.size(); ++i)
    {
        bufferStateTransitions.emplace_back(
            *rhiBufferBindings[i].buffer,
            i < bufferWriteCount ? RHIBufferState::ShaderReadWrite : RHIBufferState::ShaderResource);

        if (rhiBufferBindings[i].buffer->NeedsStagingBufferCopy())
        {
            bufferStagingCopyTransitions.emplace_back(*rhiBufferBindings[i].buffer, RHIBufferState::CopyDest);
            bufferStagingCopyTransitions.emplace_back(*rhiBufferBindings[i].buffer->GetStagingBuffer(),
                                                      RHIBufferState::CopySource);
        }
    }

    cmdList->Transition(bufferStagingCopyTransitions);
    for (int i = 0; i < bufferStagingCopyTransitions.size() / 2; ++i)
    {
        auto& buffer = bufferStagingCopyTransitions[i * 2].first;
        cmdList->Copy(*buffer.GetStagingBuffer(), buffer);
        buffer.SetNeedsStagingBufferCopy(false);
    }

    ComputePipelineStateKey psoKey = { .computeShader = shader };
    if (!cachedComputePSOKey || psoKey != cachedComputePSOKey)
    {
        // Register shader and get Pipeline if exists (if not create it).
        auto pipelineState =
            backend->GetPipelineStateCache().GetComputePipelineState(psoKey, { rhiTextureBindings, rhiBufferBindings });

        // Nothing more to do if the PSO is invalid.
        if (!pipelineState)
        {
            VEX_LOG(Error, "PSO cache returned an invalid pipeline state, unable to continue dispatch...");
            return;
        }
        cmdList->SetPipelineState(*pipelineState);
        cachedComputePSOKey = psoKey;
    }

    // Transition our resources to the correct states.
    cmdList->Transition(textureStateTransitions);
    cmdList->Transition(bufferStateTransitions);

    // Sets the resource layout to use for the dispatch
    RHIResourceLayout& resourceLayout = backend->GetPipelineStateCache().GetResourceLayout();
    resourceLayout.SetLayoutResources(backend->rhi,
                                      backend->resourceCleanup,
                                      resourceBindingSet.GetConstantBindings(),
                                      rhiTextureBindings,
                                      rhiBufferBindings,
                                      *backend->descriptorPool);
    cmdList->SetLayout(resourceLayout, *backend->descriptorPool);

    // Validate dispatch (vs platform/api constraints)
    // backend->ValidateDispatch(groupCount);

    // Perform dispatch
    cmdList->Dispatch(groupCount);
}

void CommandContext::Copy(const Texture& source, const Texture& destination)
{
    RHITexture& sourceRHI = backend->GetRHITexture(source.handle);
    RHITexture& destinationRHI = backend->GetRHITexture(destination.handle);
    std::array transitions{ std::pair<RHITexture&, RHITextureState::Flags>{ sourceRHI, RHITextureState::CopySource },
                            std::pair<RHITexture&, RHITextureState::Flags>{ destinationRHI,
                                                                            RHITextureState::CopyDest } };
    cmdList->Transition(transitions);
    cmdList->Copy(sourceRHI, destinationRHI);
}

void CommandContext::Copy(const Buffer& source, const Buffer& destination)
{
    RHIBuffer& sourceRHI = backend->GetRHIBuffer(source.handle);
    RHIBuffer& destinationRHI = backend->GetRHIBuffer(destination.handle);
    std::array transitions{ std::pair<RHIBuffer&, RHIBufferState::Flags>{ sourceRHI, RHIBufferState::CopySource },
                            std::pair<RHIBuffer&, RHIBufferState::Flags>{ destinationRHI, RHIBufferState::CopyDest } };
    cmdList->Transition(transitions);
    cmdList->Copy(sourceRHI, destinationRHI);
}

void CommandContext::SetRenderTarget(const ResourceBinding& renderTarget)
{
    if (!renderTarget.IsTexture())
    {
        VEX_LOG(Fatal, "Only textures can be set as render targets.");
    }

    if (!(renderTarget.texture.description.usage & TextureUsage::RenderTarget))
    {
        VEX_LOG(Fatal, "Only textures with RenderTarget usage set upon creation can be set as render targets.");
    }

    auto& rt = backend->GetRHITexture(renderTarget.texture.handle);
    cmdList->Transition(rt, RHITextureState::RenderTarget);

    cmdList->BeginRendering({ .renderTargets = { RHITextureBinding{ .binding = renderTarget,
                                                                    .usage = TextureUsage::RenderTarget,
                                                                    .texture = &rt } },
                              .depthStencil = std::nullopt });
#if VEX_VULKAN
    // TODO: Figure out the logic behind the user-binding api for draw calls.
    VEX_NOT_YET_IMPLEMENTED();
#endif
    cmdList->EndRendering();
}

void CommandContext::Transition(const Texture& texture, RHITextureState::Type newState)
{
    cmdList->Transition(backend->GetRHITexture(texture.handle), newState);
}

void CommandContext::Transition(const Buffer& buffer, RHIBufferState::Type newState)
{
    cmdList->Transition(backend->GetRHIBuffer(buffer.handle), newState);
}

RHICommandList& CommandContext::GetRHICommandList()
{
    return *cmdList;
}

} // namespace vex