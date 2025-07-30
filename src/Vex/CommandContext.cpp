#include "CommandContext.h"

#include <RHITypes.h>
#include <Vex/Bindings.h>
#include <Vex/Debug.h>
#include <Vex/DrawHelpers.h>
#include <Vex/GfxBackend.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/RHI/RHIBindings.h>
#include <Vex/RHI/RHICommandList.h>
#include <Vex/RHI/RHIPipelineState.h>
#include <Vex/RHI/RHISwapChain.h>
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
    ResourceBinding::ValidateResourceBindings(drawResources.readResources, ResourceUsage::Read);
    ResourceBinding::ValidateResourceBindings(drawResources.unorderedAccessResources, ResourceUsage::UnorderedAccess);
    ResourceBinding::ValidateResourceBindings(drawResources.renderTargets, ResourceUsage::RenderTarget);
    if (drawResources.depthStencil)
    {
        ResourceBinding::ValidateResourceBindings({ { *drawResources.depthStencil } }, ResourceUsage::DepthStencil);
    }
}

} // namespace CommandContext_Internal

CommandContext::CommandContext(GfxBackend* backend, RHICommandList* cmdList)
    : backend(backend)
    , cmdList(cmdList)
{
    cmdList->Open();
    // TODO: To be discussed
    RHIResourceLayout& resLayout = backend->GetPipelineStateCache().GetResourceLayout();
    cmdList->SetDescriptorPool(*backend->descriptorPool, resLayout);
    cmdList->SetLayout(resLayout);
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
    if (binding.texture.description.usage & ResourceUsage::RenderTarget)
    {
        newState = RHITextureState::RenderTarget;
    }
    else if (binding.texture.description.usage & ResourceUsage::DepthStencil)
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

    auto CollectResources = [backend = backend, &rhiTextureBindings, &rhiBufferBindings, &textureStateTransitions](
                                std::span<const ResourceBinding> resources,
                                ResourceUsage::Type usage,
                                RHITextureState::Flags state)
    {
        for (const ResourceBinding& binding : resources)
        {
            if (binding.IsTexture())
            {
                auto& texture = backend->GetRHITexture(binding.texture.handle);
                textureStateTransitions.emplace_back(texture, state);
                rhiTextureBindings.emplace_back(binding, usage, &texture);
            }

            if (binding.IsBuffer())
            {
                auto& buffer = backend->GetRHIBuffer(binding.buffer.handle);
                rhiBufferBindings.emplace_back(binding, usage, &buffer);
            }
        }
    };
    CollectResources(drawResources.readResources, ResourceUsage::Read, RHITextureState::ShaderResource);
    CollectResources(drawResources.unorderedAccessResources,
                     ResourceUsage::UnorderedAccess,
                     RHITextureState::UnorderedAccess);
    CollectResources(drawResources.renderTargets, ResourceUsage::RenderTarget, RHITextureState::RenderTarget);
    if (drawResources.depthStencil)
    {
        CollectResources({ { *drawResources.depthStencil } }, ResourceUsage::DepthStencil, RHITextureState::DepthWrite);
    }

    // Transition our resources to the correct states.
    cmdList->Transition(textureStateTransitions);

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
                              std::span<const ConstantBinding> constants,
                              std::span<const ResourceBinding> reads,
                              std::span<const ResourceBinding> readWrites,
                              std::array<u32, 3> groupCount)
{
    ResourceBinding::ValidateResourceBindings(reads, ResourceUsage::Read);
    ResourceBinding::ValidateResourceBindings(readWrites, ResourceUsage::UnorderedAccess);

    RHIResourceLayout& resourceLayout = backend->GetPipelineStateCache().GetResourceLayout();

    // Currently constants are not handled correctly, this will come with the implementation of buffers (includes
    // constant buffers).
    if (!constants.empty())
    {
        VEX_NOT_YET_IMPLEMENTED();
    }

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

    // Upload local constants as push/root constants
    // TODO: handle shader constants (validation, slot binding, etc...)!
    // This current logic is "placeholder" and more of a proof of concept than something actually useable.
    cmdList->SetLayoutLocalConstants(resourceLayout, constants);

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
    //              AND FOR THE FUTURE:
    //              Send it to the pipeline state to make it accessible to the shader via some code gen upon
    //              compilation. EG: in your shader for uniform bindless resources you'd declare VEX_RESOURCE(type,
    //              name), and the shader compiler would arrange for this to correctly get the resource using the
    //              bindless index.

    // Collect all underlying RHI textures.
    i32 totalSize = static_cast<i32>(reads.size() + readWrites.size());

    std::vector<RHITextureBinding> rhiTextureBindings;
    rhiTextureBindings.reserve(totalSize);
    std::vector<RHIBufferBinding> rhiBufferBindings;
    rhiBufferBindings.reserve(totalSize);

    std::vector<std::pair<RHITexture&, RHITextureState::Flags>> textureStateTransitions;
    textureStateTransitions.reserve(totalSize);

    auto CollectResources = [backend = backend, &rhiTextureBindings, &rhiBufferBindings, &textureStateTransitions](
                                std::span<const ResourceBinding> resources,
                                ResourceUsage::Type usage,
                                RHITextureState::Flags state)
    {
        for (const auto& binding : resources)
        {
            if (binding.IsTexture())
            {
                auto& texture = backend->GetRHITexture(binding.texture.handle);
                textureStateTransitions.emplace_back(texture, state);
                rhiTextureBindings.emplace_back(binding, usage, &texture);
            }

            if (binding.IsBuffer())
            {
                auto& buffer = backend->GetRHIBuffer(binding.buffer.handle);
                rhiBufferBindings.emplace_back(binding, usage, &buffer);
            }
        }
    };
    CollectResources(reads, ResourceUsage::Read, RHITextureState::ShaderResource);
    CollectResources(readWrites, ResourceUsage::UnorderedAccess, RHITextureState::UnorderedAccess);

    // Transition our resources to the correct states.
    cmdList->Transition(textureStateTransitions);

    // Transforms ResourceBinding into platform specific views, then binds them to the shader (preferably bindlessly).
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
    std::array<std::pair<RHITexture&, RHITextureState::Flags>, 2> transitions{
        std::pair<RHITexture&, RHITextureState::Flags>{ sourceRHI, RHITextureState::CopySource },
        std::pair<RHITexture&, RHITextureState::Flags>{ destinationRHI, RHITextureState::CopyDest }
    };
    cmdList->Transition(transitions);
    cmdList->Copy(sourceRHI, destinationRHI);
}

} // namespace vex