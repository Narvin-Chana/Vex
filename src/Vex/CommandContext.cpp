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
                                              BufferUsage::ShaderRead);
    ResourceBinding::ValidateResourceBindings(drawResources.unorderedAccessResources,
                                              TextureUsage::ShaderReadWrite,
                                              BufferUsage::ShaderReadWrite);
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
    RHITextureState::Type newState;
    if (binding.texture.description.usage & TextureUsage::RenderTarget)
    {
        newState = RHITextureState::RenderTarget;
    }
    else if (binding.texture.description.usage & TextureUsage::DepthStencil)
    {
        newState = RHITextureState::DepthWrite;
    }
    else
    {
        VEX_LOG(Fatal,
                "Invalid texture passed to ClearTexture, your texture must allow for either RenderTarget usage or "
                "DepthStencil usage.");
    }
    cmdList->Transition(texture, newState);
    cmdList->ClearTexture(
        texture,
        binding,
        optionalTextureClearValue ? *optionalTextureClearValue : binding.texture.description.clearValue);
}

void CommandContext::Draw(const DrawDescription& drawDesc, const DrawResources& drawResources, u32 vertexCount)
{
    using namespace vex::CommandContext_Internal;

    ValidateDrawResources(drawResources);

    RHIResourceLayout& resourceLayout = backend->GetPipelineStateCache().GetResourceLayout();

    if (!drawResources.constants.empty())
    {
        // Constants not yet implemented!
        VEX_NOT_YET_IMPLEMENTED();
    }

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
                                  BufferUsage::Type usage,
                                  RHIBufferState::Flags state)
        {
            for (const ResourceBinding& binding : resources)
            {
                if (!binding.IsBuffer())
                {
                    continue;
                }
                auto& buffer = backend->GetRHIBuffer(binding.buffer.handle);
                bufferStateTransitions.emplace_back(buffer, state);
                rhiBufferBindings.emplace_back(binding, usage, &buffer);
            }
        };
        CollectBuffers(drawResources.readResources, BufferUsage::ShaderRead, RHIBufferState::ShaderResource);
        CollectBuffers(drawResources.unorderedAccessResources,
                       BufferUsage::ShaderReadWrite,
                       RHIBufferState::ShaderReadWrite);
    }

    // TODO(https://trello.com/c/IGxuLci9): Correctly integrate buffers here, including staging upload.

    // Transition our resources to the correct states.
    cmdList->Transition(textureStateTransitions);
    cmdList->Transition(bufferStateTransitions);

    // Bind the layout to the pipeline
    cmdList->SetLayout(resourceLayout);

    // Bind the layout to the pipeline
    cmdList->SetLayout(resourceLayout);

    // Transforms ResourceBinding into platform specific views, then binds them to the shader (preferably bindlessly).
    cmdList->SetLayoutResources(resourceLayout, rhiTextureBindings, rhiBufferBindings, *backend->descriptorPool);

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

    if (!cachedInputAssembly || drawDesc.inputAssembly != cachedInputAssembly)
    {
        cmdList->SetInputAssembly(drawDesc.inputAssembly);
        cachedInputAssembly = drawDesc.inputAssembly;
    }

    // TODO: Validate draw vertex count (eg: versus the currently used index buffer size)

    cmdList->Draw(vertexCount);
}

void CommandContext::Dispatch(const ShaderKey& shader,
                              const ResourceBindingSet& resourceBindingSet,
                              std::array<u32, 3> groupCount)
{
    resourceBindingSet.ValidateBindings();

    RHIResourceLayout& resourceLayout = backend->GetPipelineStateCache().GetResourceLayout();

    // TODO: About resource binding logic!
    //
    // Currently constants and bindless indices are both bound as push/root constants, this could cause problems in DX12
    // if the total size of local constants, textures and global constants, surpasses the total size of the root
    // constants...
    //
    // A solution for this could be to bind bindless indices via a root constant buffer, which would allow us have an
    // "infinite" amount of them (not really infinite, the max size of a cbuffer is 64'000 bytes, a bindless handle is
    // 32 bits, so this gives us 16'000 resources that can be bound inside one shader! I believe this should be
    // sufficient for most cases!). This could potentially even be done dynamically (so only if the root constants
    // doesn't have enough space to fit all resources), this would allow us to avoid the additional indirection this has
    // when few resources are bound!

    // --------------------------------
    // Current Resource Binding Logic:
    // --------------------------------
    //
    // Iterate on ResourceBindings in reads/writes:
    //      Collect RHITextures and RHIBuffers with their associated ResourceBinding.
    // Pass the collected resource-binding pairs to the RHICommandList:
    //      Create a platform-specific view from the resource-binding pair.
    //      Determine if this view already exists in the underlying RHIResource's ViewCache.
    //      Determine if the resource can be bound bindlessly or not:
    //          If it is a non-bindless resource:
    //              Bind it directly to the pipeline and we're done (eg: output images, depth stencil in graphics
    //              pipeline).
    //          Else:
    //              Grab its bindless handle:
    //                  Make sure its still valid:
    //                      If so we can use it.
    //                      Else create a new bindless handle that will be used (TODO: figure out logic for dynamic vs
    //                      static resources, for now we only consider static resources).
    //              Once the bindless handle has been obtained send it to the command list to be set as a push/root
    //              constant.
    //
    //              Right now these constants will be "guessed" by the user in the shader-side (probably bound to slots
    //              in the order of declaration on C++ side).
    //
    //              Then we send it to the pipeline state to make it accessible to the shader via some code gen upon
    //              compilation. EG: in shaders for uniform bindless resources you'd declare VEX_RESOURCE(type,
    //              name), and the shader compiler would arrange for this to correctly get the resource using the
    //              bindless index.

    // Collect all underlying RHI textures.
    std::vector<RHITextureBinding> rhiTextureBindings;
    std::vector<RHIBufferBinding> rhiBufferBindings;
    ResourceBindingSet::CollectRHIResources(*backend,
                                            resourceBindingSet.writes,
                                            rhiTextureBindings,
                                            rhiBufferBindings,
                                            TextureUsage::ShaderReadWrite,
                                            BufferUsage::ShaderReadWrite);
    const u32 texWriteCount = rhiTextureBindings.size();
    const u32 bufferWriteCount = rhiBufferBindings.size();
    ResourceBindingSet::CollectRHIResources(*backend,
                                            resourceBindingSet.reads,
                                            rhiTextureBindings,
                                            rhiBufferBindings,
                                            TextureUsage::ShaderRead,
                                            BufferUsage::ShaderRead);

    // This code will be greatly simplied when we add caching of transitions until the next GPU operation.
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

    // Transition our resources to the correct states.
    cmdList->Transition(textureStateTransitions);
    cmdList->Transition(bufferStateTransitions);

    // Sets the resource layout to use for the dispatch
    cmdList->SetLayout(resourceLayout);

    // Upload local constants as push/root constants
    // TODO: handle shader constants (validation, slot binding, etc...)!
    cmdList->SetLayoutLocalConstants(resourceLayout, resourceBindingSet.GetConstantBindings());

    cmdList->SetLayoutResources(resourceLayout, rhiTextureBindings, rhiBufferBindings, *backend->descriptorPool);

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

} // namespace vex