#include "CommandContext.h"

#include <Vex/Bindings.h>
#include <Vex/GfxBackend.h>
#include <Vex/RHI/RHICommandList.h>
#include <Vex/RHI/RHISwapChain.h>

namespace vex
{

CommandContext::CommandContext(GfxBackend* backend, RHICommandList* cmdList)
    : backend(backend)
    , cmdList(cmdList)
{
    cmdList->Open();
    cmdList->SetLayout(backend->GetPipelineStateCache().GetResourceLayout());
}

CommandContext::~CommandContext()
{
    backend->EndCommandContext(*cmdList);
}

void CommandContext::Draw(const DrawDescription& drawDesc,
                          std::span<const ConstantBinding> constants,
                          std::span<const ResourceBinding> reads,
                          std::span<const ResourceBinding> writes,
                          u32 vertexCount)
{
}

void CommandContext::Dispatch(const ShaderKey& shader,
                              std::span<const ConstantBinding> constants,
                              std::span<const ResourceBinding> reads,
                              std::span<const ResourceBinding> writes,
                              std::array<u32, 3> groupCount)
{
    // Register shader and get Pipeline if exists (if not create it).
    auto pipelineState = backend->GetPipelineStateCache().GetComputePipelineState({ .computeShader = shader });
    cmdList->SetPipelineState(*pipelineState);

    // Upload local constants as push/root constants
    cmdList->SetLayoutLocalConstants(backend->GetPipelineStateCache().GetResourceLayout(), constants);

    // TODO: implement textures properly
    // Validate reads and writes, get bindable objects
    // auto readSRVs = backend->Validate(reads);
    // auto writeSRVs = backend->Validate(writes);

    // TODO: figure out how we want to bind textures (if we even want to, an option would be to go fully bindless and
    // disallow the old bindful model!)

    // Validate dispatch (vs platform/api constraints)
    // backend->ValidateDispatch(groupCount);

    // Perform dispatch
    // TEMP: second and third parameters are just to copy our dispatch result to the backbuffer...
    // Obviously should be removed once we have a functional Copy method.
    cmdList->Dispatch(groupCount,
                      backend->GetPipelineStateCache().GetResourceLayout(),
                      *backend->GetRHITexture(backend->GetCurrentBackBuffer().handle));
}

#include "Debug.h"

void CommandContext::Copy(const Texture& source, const Texture& destination)
{
    VEX_NOT_YET_IMPLEMENTED();
}

} // namespace vex