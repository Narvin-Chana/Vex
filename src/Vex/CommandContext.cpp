#include "CommandContext.h"

#include <algorithm>
#include <variant>

#include <Vex/Containers/Utils.h>
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

namespace vex
{

namespace CommandContext_Internal
{

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

static BufferDescription GetStagingBufferDescription(const std::string& name, u32 byteSize)
{
    BufferDescription description;
    description.byteSize = byteSize;
    description.name = std::string(name) + "_staging";
    description.usage = BufferUsage::None;
    description.memoryLocality = ResourceMemoryLocality::CPUWrite;
    return description;
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
    cmdList->ClearTexture({ binding, texture },
                          // This is a safe cast, textures can only contain one of the two usages (RT/DS).
                          static_cast<TextureUsage::Type>(binding.texture.description.usage &
                                                          (TextureUsage::RenderTarget | TextureUsage::DepthStencil)),
                          textureClearValue.value_or(binding.texture.description.clearValue));
}

void CommandContext::BeginRendering(const DrawResourceBinding& drawBindings)
{
    if (currentDrawResources.has_value())
    {
        VEX_LOG(Fatal, "BeginRendering must never be called twice without calling EndRendering first.");
    }

    currentDrawResources.emplace();
    std::vector<std::pair<RHITexture&, RHITextureState::Flags>> transitions;
    for (const auto& renderTarget : drawBindings.renderTargets)
    {
        transitions.emplace_back(backend->GetRHITexture(renderTarget.texture.handle), RHITextureState::RenderTarget);
        currentDrawResources->renderTargets.emplace_back(renderTarget,
                                                         backend->GetRHITexture(renderTarget.texture.handle));
    }

    if (drawBindings.depthStencil)
    {
        currentDrawResources->depthStencil = { *drawBindings.depthStencil,
                                               backend->GetRHITexture(drawBindings.depthStencil->texture.handle) };
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

void CommandContext::Draw(const DrawDescription& drawDesc, std::optional<ConstantBinding> constants, u32 vertexCount)
{
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

    auto graphicsPSOKey = CommandContext_Internal::GetGraphicsPSOKeyFromDrawDesc(drawDesc, *currentDrawResources);

    if (!cachedGraphicsPSOKey || graphicsPSOKey != *cachedGraphicsPSOKey)
    {
        const RHIGraphicsPipelineState* pipelineState = backend->psCache.GetGraphicsPipelineState(graphicsPSOKey);
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
    resourceLayout.SetLayoutResources(constants);

    cmdList->SetLayout(resourceLayout);

    if (!cachedInputAssembly || drawDesc.inputAssembly != cachedInputAssembly)
    {
        cmdList->SetInputAssembly(drawDesc.inputAssembly);
        cachedInputAssembly = drawDesc.inputAssembly;
    }

    // TODO(https://trello.com/c/IGxuLci9): Validate draw vertex count (eg: versus the currently used index buffer size)

    cmdList->Draw(vertexCount);
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

void CommandContext::Copy(const Buffer& source, const Texture& destination)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void CommandContext::EnqueueDataUpload(const Buffer& buffer, std::span<const u8> data)
{
    if (buffer.description.memoryLocality == ResourceMemoryLocality::CPUWrite)
    {
        RHIBuffer& rhiDestBuffer = backend->GetRHIBuffer(buffer.handle);
        ResourceMappedMemory(rhiDestBuffer).SetData(data);
        return;
    }

    Buffer stagingBuffer = backend->CreateBuffer(
        CommandContext_Internal::GetStagingBufferDescription(buffer.description.name, buffer.description.byteSize),
        ResourceLifetime::Static);

    RHIBuffer& rhiDestBuffer = backend->GetRHIBuffer(buffer.handle);
    RHIBuffer& rhiStagingBuffer = backend->GetRHIBuffer(stagingBuffer.handle);

    ResourceMappedMemory(rhiStagingBuffer).SetData(data);

    cmdList->Transition(rhiStagingBuffer, RHIBufferState::CopySource);
    cmdList->Transition(rhiDestBuffer, RHIBufferState::CopyDest);
    cmdList->Copy(rhiStagingBuffer, rhiDestBuffer);

    backend->DestroyBuffer(stagingBuffer);
}

void CommandContext::EnqueueDataUpload(const Texture& texture, std::span<const u8> data)
{
    Buffer stagingBuffer = backend->CreateBuffer(
        CommandContext_Internal::GetStagingBufferDescription(texture.description.name,
                                                             texture.description.GetTextureByteSize()),
        ResourceLifetime::Static);

    RHIBuffer& rhiStagingBuffer = backend->GetRHIBuffer(stagingBuffer.handle);
    RHITexture& rhiDestTexture = backend->GetRHITexture(texture.handle);

    ResourceMappedMemory(rhiStagingBuffer).SetData(data);

    cmdList->Transition(rhiStagingBuffer, RHIBufferState::CopySource);
    cmdList->Transition(rhiDestTexture, RHIBufferState::CopyDest);
    cmdList->Copy(rhiStagingBuffer, rhiDestTexture);

    backend->DestroyBuffer(stagingBuffer);
}

BindlessHandle CommandContext::GetBindlessHandle(const ResourceBinding& resourceBinding)
{
    vex::BindlessHandle handle;
    std::visit(Visitor{ [&handle, backend = backend](const BufferBinding& bufferBinding)
                        { handle = backend->GetBufferBindlessHandle(bufferBinding); },
                        [&handle, backend = backend](const TextureBinding& texBinding)
                        { handle = backend->GetTextureBindlessHandle(texBinding); } },
               resourceBinding.binding);
    return handle;
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
    CommandContext_Internal::TransitionBindings(*cmdList, rhiTextureBindings);
    CommandContext_Internal::TransitionBindings(*cmdList, rhiBufferBindings);
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