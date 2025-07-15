#include "CommandContext.h"

#include "RHI/RHIResourceLayout.h"

#include <Vex/Bindings.h>
#include <Vex/Debug.h>
#include <Vex/GfxBackend.h>
#include <Vex/Logger.h>
#include <Vex/RHI/RHIBindings.h>
#include <Vex/RHI/RHICommandList.h>
#include <Vex/RHI/RHISwapChain.h>
#include <Vex/ResourceBindingSet.h>
#include <Vex/ShaderResourceContext.h>

namespace vex
{

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

void CommandContext::Draw(const DrawDescription& drawDesc,
                          const ResourceBindingSet& resourceBindingSet,
                          u32 vertexCount)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void CommandContext::Dispatch(const ShaderKey& shader,
                              const ResourceBindingSet& resourceBindingSet,
                              std::array<u32, 3> groupCount)
{
    resourceBindingSet.ValidateBindings();

    RHIResourceLayout& resourceLayout = backend->GetPipelineStateCache().GetResourceLayout();

    resourceLayout.Update(resourceBindingSet);

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
    //              AND FOR THE FUTURE:
    //              Send it to the pipeline state to make it accessible to the shader via some code gen upon
    //              compilation. EG: in your shader for uniform bindless resources you'd declare VEX_RESOURCE(type,
    //              name), and the shader compiler would arrange for this to correctly get the resource using the
    //              bindless index.

    // Collect all underlying RHI textures.

    std::vector<RHITextureBinding> rhiTextureBindings;
    std::vector<RHIBufferBinding> rhiBufferBindings;
    resourceBindingSet.CollectWriteBindings(*backend, rhiTextureBindings, rhiBufferBindings);
    const u32 texWriteCount = rhiTextureBindings.size();
    resourceBindingSet.CollectReadBindings(*backend, rhiTextureBindings, rhiBufferBindings);

    std::vector<std::pair<RHITexture&, RHITextureState::Flags>> textureStateTransitions;
    textureStateTransitions.reserve(rhiTextureBindings.size());

    for (u32 i = 0; i < rhiTextureBindings.size(); ++i)
    {
        textureStateTransitions.emplace_back(
            *rhiTextureBindings[i].texture,
            i < texWriteCount ? RHITextureState::UnorderedAccess : RHITextureState::ShaderResource);
    }

    // Transition our resources to the correct states.
    cmdList->Transition(textureStateTransitions);

    // Sets the resource layout to use for the dispatch
    cmdList->SetLayout(resourceLayout);

    // Upload local constants as push/root constants
    // TODO: handle shader constants (validation, slot binding, etc...)!
    cmdList->SetLayoutLocalConstants(resourceLayout, resourceBindingSet.GetConstantBindings());

    cmdList->SetLayoutResources(resourceLayout, rhiTextureBindings, rhiBufferBindings, *backend->descriptorPool);

    // Register shader and get Pipeline if exists (if not create it).
    // TODO: If we would want to add the shader code-gen to fill-in bindless resources automatically, we'd also have to
    // pass in the reads, writes and constants here.
    auto pipelineState =
        backend->GetPipelineStateCache().GetComputePipelineState({ .computeShader = shader },
                                                                 { rhiTextureBindings, rhiBufferBindings });

    if (!pipelineState)
    {
        return;
    }
    cmdList->SetPipelineState(*pipelineState);

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