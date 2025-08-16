#include "CommandContext.h"

#include <algorithm>

#include <Vex/Debug.h>
#include <Vex/DrawHelpers.h>
#include <Vex/GfxBackend.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/RHIBindings.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHIResourceLayout.h>
#include <Vex/RHIImpl/RHISwapChain.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/RayTracing.h>
#include <Vex/ResourceBindingUtils.h>
#include <Vex/ShaderResourceContext.h>

namespace vex
{

namespace CommandContext_Internal
{

static void TransitionAndCopyFromStaging(RHICommandList& cmdList,
                                         const std::vector<RHIBufferBinding>& rhiBufferBindings)
{
    std::vector<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferStagingCopyTransitions;
    for (const auto& bufferBinding : rhiBufferBindings)
    {
        if (bufferBinding.buffer->NeedsStagingBufferCopy())
        {
            bufferStagingCopyTransitions.emplace_back(*bufferBinding.buffer, RHIBufferState::CopyDest);
            bufferStagingCopyTransitions.emplace_back(*bufferBinding.buffer->GetStagingBuffer(),
                                                      RHIBufferState::CopySource);
        }
    }

    cmdList.Transition(bufferStagingCopyTransitions);
    for (int i = 0; i < bufferStagingCopyTransitions.size() / 2; ++i)
    {
        auto& buffer = bufferStagingCopyTransitions[i * 2].first;
        cmdList.Copy(*buffer.GetStagingBuffer(), buffer);
        buffer.SetNeedsStagingBufferCopy(false);
    }
}

static void TransitionBindings(RHICommandList& cmdList, const std::vector<RHITextureBinding>& rhiTextureBindings)
{
    std::vector<std::pair<RHITexture&, RHITextureState::Flags>> textureStateTransitions;
    textureStateTransitions.reserve(rhiTextureBindings.size());
    for (const auto& texBinding : rhiTextureBindings)
    {
        textureStateTransitions.emplace_back(*texBinding.texture,
                                             ResourceBindingUtils::TextureBindingUsageToState(
                                                 static_cast<TextureUsage::Type>(texBinding.binding.usage)));
    }

    cmdList.Transition(textureStateTransitions);
}

static void TransitionBindings(RHICommandList& cmdList, const std::vector<RHIBufferBinding>& rhiBufferBindings)
{
    std::vector<std::pair<RHIBuffer&, RHIBufferState::Flags>> bufferStateTransitions;
    bufferStateTransitions.reserve(rhiBufferBindings.size());
    for (const auto& bufferBinding : rhiBufferBindings)
    {
        bufferStateTransitions.emplace_back(
            *bufferBinding.buffer,
            ResourceBindingUtils::BufferBindingUsageToState(bufferBinding.binding.usage));
    }

    cmdList.Transition(bufferStateTransitions);
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

    key.renderTargetState.depthStencilFormat = rhiDrawRes.depthStencil.has_value()
                                                   ? rhiDrawRes.depthStencil->binding.texture.description.format
                                                   : TextureFormat::UNKNOWN;

    // Ensure each render target has atleast a default color attachment (no blending, write all).
    key.colorBlendState.attachments.resize(rhiDrawRes.renderTargets.size());

    return key;
}

} // namespace CommandContext_Internal

CommandContext::CommandContext(GfxBackend* backend, RHICommandList* cmdList)
    : backend(backend)
    , cmdList(cmdList)
{
    cmdList->Open();
    cmdList->SetDescriptorPool(*backend->descriptorPool, backend->psCache.GetResourceLayout());
}

CommandContext::~CommandContext()
{
    if (currentDrawResources.has_value())
    {
        VEX_LOG(Fatal,
                "The command context was closed with a still open rendering pass! You might have forgotten to call "
                "EndRendering()!");
    }
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

    RHITexture& texture = backend->GetRHITexture(binding.texture.handle);
    cmdList->Transition(texture, texture.GetClearTextureState());
    cmdList->ClearTexture({ binding, &texture },
                          // This is a safe cast, textures can only contain one of the two usages (RT/DS).
                          static_cast<TextureUsage::Type>(binding.texture.description.usage &
                                                          (TextureUsage::RenderTarget | TextureUsage::DepthStencil)),
                          textureClearValue.value_or(binding.texture.description.clearValue));
}
void CommandContext::BeginRendering(const DrawResourceBinding& drawBindings)
{
    if (currentDrawResources.has_value())
    {
        VEX_LOG(Fatal, "BeginRendering must never be called twice without calling EndRendering first");
    }

    currentDrawResources.emplace();
    std::vector<std::pair<RHITexture&, RHITextureState::Flags>> transitions;
    for (const auto& renderTarget : drawBindings.renderTargets)
    {
        transitions.emplace_back(backend->GetRHITexture(renderTarget.texture.handle), RHITextureState::RenderTarget);
        currentDrawResources->renderTargets.emplace_back(renderTarget,
                                                         &backend->GetRHITexture(renderTarget.texture.handle));
    }


    if (drawBindings.depthStencil)
    {
        currentDrawResources->depthStencil = { *drawBindings.depthStencil,
                                               &backend->GetRHITexture(drawBindings.depthStencil->texture.handle) };
        transitions.emplace_back(backend->GetRHITexture(drawBindings.depthStencil->texture.handle),
                                 RHITextureState::DepthWrite);
    }

    cmdList->Transition(transitions);
    cmdList->BeginRendering(*currentDrawResources);
}
void CommandContext::EndRendering()
{
    if (!currentDrawResources.has_value())
    {
        VEX_LOG(Fatal, "BeginRendering must have been called before a call to EndRendering is valid");
    }

    cmdList->EndRendering();

    currentDrawResources.reset();
}

void CommandContext::Draw(const DrawDescription& drawDesc, const DrawResources& drawResources, u32 vertexCount)
{
    using namespace vex::CommandContext_Internal;

    if (!currentDrawResources)
    {
        VEX_LOG(Fatal, "BeginRender must be called before any draw call is executed");
    }

    if (drawDesc.vertexShader.type != ShaderType::VertexShader)
    {
        VEX_LOG(Fatal,
                "Invalid type passed to Draw call for vertex shader: {}",
                magic_enum::enum_name(drawDesc.vertexShader.type));
    }
    if (drawDesc.pixelShader.type != ShaderType::PixelShader)
    {
        VEX_LOG(Fatal,
                "Invalid type passed to Draw call for pixel shader: {}",
                magic_enum::enum_name(drawDesc.pixelShader.type));
    }

    ResourceBindingUtils::ValidateResourceBindings(drawResources.resourceBindings);

    // Collect all underlying RHI textures.
    u32 totalSize = static_cast<u32>(drawResources.resourceBindings.size());

    // Conservative allocation.
    std::vector<RHITextureBinding> rhiTextureBindings;
    rhiTextureBindings.reserve(totalSize);
    std::vector<RHIBufferBinding> rhiBufferBindings;
    rhiBufferBindings.reserve(totalSize);

    ResourceBindingUtils::CollectRHIResources(*backend,
                                              drawResources.resourceBindings,
                                              rhiTextureBindings,
                                              rhiBufferBindings);

    TransitionAndCopyFromStaging(*cmdList, rhiBufferBindings);
    TransitionBindings(*cmdList, rhiTextureBindings);
    TransitionBindings(*cmdList, rhiBufferBindings);

    auto graphicsPSOKey = GetGraphicsPSOKeyFromDrawDesc(drawDesc, *currentDrawResources);

    if (!cachedGraphicsPSOKey || graphicsPSOKey != *cachedGraphicsPSOKey)
    {
        const RHIGraphicsPipelineState* pipelineState =
            backend->psCache.GetGraphicsPipelineState(graphicsPSOKey,
                                                      ShaderResourceContext{ rhiTextureBindings, rhiBufferBindings });
        // No valid PSO means we cannot proceed.
        if (!pipelineState)
        {
            return;
        }

        cmdList->SetPipelineState(*pipelineState);
        cachedGraphicsPSOKey = graphicsPSOKey;
    }

    // Setup the layout for our pass.
    RHIResourceLayout& resourceLayout = backend->psCache.GetResourceLayout();
    resourceLayout.SetLayoutResources(backend->rhi,
                                      backend->resourceCleanup,
                                      drawResources.constants,
                                      rhiTextureBindings,
                                      rhiBufferBindings,
                                      *backend->descriptorPool,
                                      *backend->allocator);

    cmdList->SetLayout(resourceLayout, *backend->descriptorPool);

    if (!cachedInputAssembly || drawDesc.inputAssembly != cachedInputAssembly)
    {
        cmdList->SetInputAssembly(drawDesc.inputAssembly);
        cachedInputAssembly = drawDesc.inputAssembly;
    }

    // TODO(https://trello.com/c/IGxuLci9): Validate draw vertex count (eg: versus the currently used index buffer size)

    cmdList->Draw(vertexCount);
}

void CommandContext::Dispatch(const ShaderKey& shader,
                              std::span<const ResourceBinding> resourceBindings,
                              const std::optional<ConstantBinding>& constants,
                              std::array<u32, 3> groupCount)
{
    if (shader.type != ShaderType::ComputeShader)
    {
        VEX_LOG(Fatal, "Invalid shader type passed to Dispatch call: {}", magic_enum::enum_name(shader.type));
    }

    using namespace CommandContext_Internal;

    // Collect all underlying RHI textures.
    std::vector<RHITextureBinding> rhiTextureBindings;
    std::vector<RHIBufferBinding> rhiBufferBindings;
    ResourceBindingUtils::CollectRHIResources(*backend, resourceBindings, rhiTextureBindings, rhiBufferBindings);

    // This code will be greatly simplified when we add caching of transitions until the next GPU operation.
    // See: https://trello.com/c/kJWhd2iu
    TransitionAndCopyFromStaging(*cmdList, rhiBufferBindings);
    TransitionBindings(*cmdList, rhiTextureBindings);
    TransitionBindings(*cmdList, rhiBufferBindings);

    ComputePipelineStateKey psoKey = { .computeShader = shader };
    if (!cachedComputePSOKey || psoKey != cachedComputePSOKey)
    {
        // Register shader and get Pipeline if exists (if not create it).
        const RHIComputePipelineState* pipelineState =
            backend->psCache.GetComputePipelineState(psoKey, { rhiTextureBindings, rhiBufferBindings });

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
    resourceLayout.SetLayoutResources(backend->rhi,
                                      backend->resourceCleanup,
                                      constants,
                                      rhiTextureBindings,
                                      rhiBufferBindings,
                                      *backend->descriptorPool,
                                      *backend->allocator);

    cmdList->SetLayout(resourceLayout, *backend->descriptorPool);

    // Validate dispatch (vs platform/api constraints)
    // backend->ValidateDispatch(groupCount);

    // Perform dispatch
    cmdList->Dispatch(groupCount);
}

void CommandContext::TraceRays(const RayTracingPassDescription& rayTracingPassDescription,
                               std::span<const ResourceBinding> resourceBindings,
                               const std::optional<ConstantBinding>& constants,
                               std::array<u32, 3> widthHeightDepth)
{
    RayTracingPassDescription::ValidateShaderTypes(rayTracingPassDescription);

    using namespace CommandContext_Internal;

    // Collect all underlying RHI textures.
    std::vector<RHITextureBinding> rhiTextureBindings;
    std::vector<RHIBufferBinding> rhiBufferBindings;
    ResourceBindingUtils::CollectRHIResources(*backend, resourceBindings, rhiTextureBindings, rhiBufferBindings);

    // This code will be greatly simplified when we add caching of transitions until the next GPU operation.
    // See: https://trello.com/c/kJWhd2iu
    TransitionAndCopyFromStaging(*cmdList, rhiBufferBindings);
    TransitionBindings(*cmdList, rhiTextureBindings);
    TransitionBindings(*cmdList, rhiBufferBindings);

    const RHIRayTracingPipelineState* pipelineState =
        backend->psCache.GetRayTracingPipelineState(rayTracingPassDescription, {});
    if (!pipelineState)
    {
        VEX_LOG(Error, "PSO cache returned an invalid pipeline state, unable to continue dispatch...");
        return;
    }
    cmdList->SetPipelineState(*pipelineState);

    // Sets the resource layout to use for the ray trace.
    RHIResourceLayout& resourceLayout = backend->psCache.GetResourceLayout();
    resourceLayout.SetLayoutResources(backend->rhi,
                                      backend->resourceCleanup,
                                      constants,
                                      rhiTextureBindings,
                                      rhiBufferBindings,
                                      *backend->descriptorPool);

    cmdList->SetLayout(resourceLayout, *backend->descriptorPool);

    // Validate ray trace (vs platform/api constraints)
    // backend->ValidateTraceRays(widthHeightDepth);

    cmdList->TraceRays(widthHeightDepth, *pipelineState);
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