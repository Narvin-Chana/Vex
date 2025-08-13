#pragma once

#include <optional>

#include <Vex/RHIBindings.h>
#include <Vex/RHIFwd.h>
#include <Vex/ShaderKey.h>
#include <Vex/Types.h>

#include <RHI/RHIBuffer.h>
#include <RHI/RHIPipelineState.h>
#include <RHI/RHITexture.h>

namespace vex
{

class GfxBackend;
struct ConstantBinding;
struct ResourceBinding;
struct Texture;
struct Buffer;
struct TextureClearValue;
struct DrawDescription;
struct DrawResources;
struct ComputeResources;

class CommandContext
{
public:
    CommandContext(GfxBackend* backend, RHICommandList* cmdList);
    ~CommandContext();

    CommandContext(const CommandContext& other) = delete;
    CommandContext& operator=(const CommandContext& other) = delete;

    CommandContext(CommandContext&& other) = default;
    CommandContext& operator=(CommandContext&& other) = default;

    void SetViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);
    void SetScissor(i32 x, i32 y, u32 width, u32 height);

    // Clears a texture, by default will use the texture's ClearColor.
    void ClearTexture(const TextureBinding& binding,
                      TextureUsage::Type clearUsage,
                      const TextureClearValue* optionalTextureClearValue =
                          nullptr, // Use ptr instead of optional to allow for fwd declaration of type.
                      std::optional<std::array<float, 4>> clearRect = std::nullopt);

    void BeginRendering(const DrawResourceBinding& drawBindings);
    void EndRendering();

    void Draw(const DrawDescription& drawDesc, const DrawResources& drawResources, u32 vertexCount);

    void DrawIndexed()
    {
    }
    void DrawInstancedIndexed()
    {
    }

    void Dispatch(const ShaderKey& shader,
                  std::span<const ResourceBinding> resourceBindings,
                  std::span<const ConstantBinding> constantBindings,
                  std::array<u32, 3> groupCount);

    void Copy(const Texture& source, const Texture& destination);
    void Copy(const Buffer& source, const Buffer& destination);

    // Allows you to transition the passed in texture to the correct state. Usually this is done automatically by Vex
    // before any draws or dispatches for the resources you pass in.
    //
    // However, in the case you are leveraging bindless resources (VEX_GET_BINDLESS_RESOURCE), you are responsible for
    // ensuring any used resources are in the correct state.
    //
    // This contains redundancy checks so feel free to call it even if the resource is potentially already in the
    // desired state for correctness.
    void Transition(const Texture& texture, RHITextureState::Type newState);

    // Allows you to transition the passed in buffer to the correct state. Usually this is done automatically by Vex
    // before any draws or dispatches for the resources you pass in.
    //
    // However, in the case you are leveraging bindless resources (VEX_GET_BINDLESS_RESOURCE), you are responsible for
    // ensuring any used resources are in the correct state.
    //
    // This contains redundancy checks so feel free to call it even if the resource is potentially already in the
    // desired state for correctness.
    void Transition(const Buffer& buffer, RHIBufferState::Type newState);

    // Returns the RHI command list associated with this context (you should avoid using this unless you know
    // what you are doing).
    RHICommandList& GetRHICommandList();

private:
    GfxBackend* backend;
    RHICommandList* cmdList;

    // Used to avoid resetting the same state multiple times which can be costly on certain hardware.
    // In general draws and dispatches are recommended to be grouped by PSO, so this caching can be very efficient
    // versus binding everything each time.
    std::optional<GraphicsPipelineStateKey> cachedGraphicsPSOKey = std::nullopt;
    std::optional<ComputePipelineStateKey> cachedComputePSOKey = std::nullopt;
    std::optional<InputAssembly> cachedInputAssembly = std::nullopt;

    std::optional<RHIDrawResources> currentDrawResources;
};

} // namespace vex