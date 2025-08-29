#pragma once

#include <optional>

#include <Vex/RHIBindings.h>
#include <Vex/RHIFwd.h>
#include <Vex/Shaders/ShaderKey.h>
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
struct RayTracingPassDescription;

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
                      std::optional<TextureClearValue> textureClearValue = std::nullopt,
                      std::optional<std::array<float, 4>> clearRect = std::nullopt);

    // Starts a graphics rendering pass, setting up the graphics pipeline for your specific draw bindings.
    // Once called you must eventually have a matching EndRendering call.
    void BeginRendering(const DrawResourceBinding& drawBindings);
    // Marks the end of a graphics rendering pass. Must have previously called BeginRendering.
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
                  const std::optional<ConstantBinding>& constants,
                  std::array<u32, 3> groupCount);

    void TraceRays(const RayTracingPassDescription& rayTracingPassDescription,
                   std::span<const ResourceBinding> resourceBindings,
                   const std::optional<ConstantBinding>& constants,
                   std::array<u32, 3> widthHeightDepth);

    void Copy(const Texture& source, const Texture& destination);
    void Copy(const Buffer& source, const Buffer& destination);
    void Copy(const Buffer& source, const Texture& destination);

    void EnqueueDataUpload(const Buffer& buffer, std::span<const u8> data);
    void EnqueueDataUpload(const Texture& buffer, std::span<const u8> data);

    template <class T>
    void EnqueueDataUpload(const Texture& texture, const T& data);
    template <class T>
    void EnqueueDataUpload(const Buffer& buffer, const T& data);

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
    std::optional<GraphicsPipelineStateKey> cachedGraphicsPSOKey;
    std::optional<ComputePipelineStateKey> cachedComputePSOKey;
    std::optional<InputAssembly> cachedInputAssembly;

    std::optional<RHIDrawResources> currentDrawResources;
};

template <class T>
void CommandContext::EnqueueDataUpload(const Texture& texture, const T& data)
{
    EnqueueDataUpload(texture, std::span{ reinterpret_cast<const u8*>(&data), sizeof(T) });
}

template <class T>
void CommandContext::EnqueueDataUpload(const Buffer& buffer, const T& data)
{
    EnqueueDataUpload(buffer, std::span{ reinterpret_cast<const u8*>(&data), sizeof(T) });
}

} // namespace vex