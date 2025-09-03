#pragma once

#include <optional>

#include <Vex/RHIBindings.h>
#include <Vex/RHIFwd.h>
#include <Vex/Shaders/ShaderKey.h>
#include <Vex/Types.h>

#include <RHI/RHIBuffer.h>
#include <RHI/RHICommandList.h>
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
    CommandContext(GfxBackend* backend, NonNullPtr<RHICommandList> cmdList);
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

    void Draw(const DrawDescription& drawDesc,
              const DrawResourceBinding& drawBindings,
              std::optional<ConstantBinding> constants,
              u32 vertexCount,
              u32 instanceCount = 1,
              u32 vertexOffset = 0,
              u32 instanceOffset = 0);

    void DrawIndexed(const DrawDescription& drawDesc,
                     const DrawResourceBinding& drawBindings,
                     std::optional<ConstantBinding> constants,
                     u32 indexCount,
                     u32 instanceCount = 1,
                     u32 indexOffset = 0,
                     u32 vertexOffset = 0,
                     u32 instanceOffset = 0);

    void Dispatch(const ShaderKey& shader,
                  const std::optional<ConstantBinding>& constants,
                  std::array<u32, 3> groupCount);

    void TraceRays(const RayTracingPassDescription& rayTracingPassDescription,
                   const std::optional<ConstantBinding>& constants,
                   std::array<u32, 3> widthHeightDepth);

    // Copies the entirety of source texture (all mips and array levels) to the destination texture
    void Copy(const Texture& source, const Texture& destination);
    // Copies a region of source texture to destination texture
    void Copy(const Texture& source, const Texture& destination, const TextureCopyDescription& regionMapping);
    // Copies multiple regions of source texture to destination texture
    void Copy(const Texture& source,
              const Texture& destination,
              std::span<const TextureCopyDescription> regionMappings);
    // Copies the entirety of source buffer to the destination buffer
    void Copy(const Buffer& source, const Buffer& destination);
    // Copies the specified region from source to destination buffer
    void Copy(const Buffer& source, const Buffer& destination, const BufferCopyDescription& regionMappings);
    // Copies the contents of a buffer to the specified texture according to API needs
    void Copy(const Buffer& source, const Texture& destination);
    // Copies the contents of the buffer to a specified region in the texture
    void Copy(const Buffer& source, const Texture& destination, const BufferToTextureCopyDescription& regionMapping);
    // Copies the contents of the buffer to multiple specified regions in the texture
    void Copy(const Buffer& source,
              const Texture& destination,
              std::span<const BufferToTextureCopyDescription> regionMappings);

    // Enqueues data to be uploaded to the specific buffer. If the buffer is mappable it will map it and directly write
    // data to it. If the buffer isn't mappable a staging buffer is used implicitly
    void EnqueueDataUpload(const Buffer& buffer, std::span<const u8> data);
    // Enqueues data to be uploaded to specific region of destination buffer using a staging buffer when necessary
    void EnqueueDataUpload(const Buffer& buffer, std::span<const u8> data, const BufferSubresource& subresource);

    // Enqueues data to be uploaded to the specific texture with the use of a staging buffer.
    void EnqueueDataUpload(const Texture& texture, std::span<const u8> data);
    // Enqueues data to be uploaded to a specific region of the texture
    void EnqueueDataUpload(const Texture& texture,
                           std::span<const u8> data,
                           const TextureSubresource& subresource,
                           const TextureExtent& extent);

    template <class T>
    void EnqueueDataUpload(const Texture& texture, const T& data);
    template <class T>
    void EnqueueDataUpload(const Texture& texture,
                           const T& data,
                           const TextureSubresource& subresource,
                           const TextureExtent& extent);
    template <class T>
    void EnqueueDataUpload(const Buffer& buffer, const T& data);

    BindlessHandle GetBindlessHandle(const ResourceBinding& resourceBinding);
    std::vector<BindlessHandle> GetBindlessHandles(std::span<const ResourceBinding> resourceBindings);

    void TransitionBindings(std::span<const ResourceBinding> resourceBindings);

    // Allows you to transition the passed in texture to the correct state. Usually this is done automatically by Vex
    // before any draws or dispatches for the resources you pass in.
    // However, in the case you are leveraging bindless resources, you are responsible for ensuring any used resources
    // are in the correct state. This contains redundancy checks so feel free to call it even if the resource is
    // potentially already in the desired state for correctness.
    void Transition(const Texture& texture, RHITextureState::Type newState);

    // Allows you to transition the passed in buffer to the correct state. Usually this is done automatically by Vex
    // before any draws or dispatches for the resources you pass in.
    // However, in the case you are leveraging bindless resources, you are responsible for ensuring any used resources
    // are in the correct state. This contains redundancy checks so feel free to call it even if the resource is
    // potentially already in the desired state for correctness.
    void Transition(const Buffer& buffer, RHIBufferState::Type newState);

    // Useful for calling native API draws when wanting to render to a specific Render Target.
    // Allows the passed in lambda to be executed in a draw scope.
    void ExecuteInDrawContext(std::span<const TextureBinding> renderTargets,
                              std::optional<const TextureBinding> depthStencil,
                              const std::function<void()>& callback);

    // Returns the RHI command list associated with this context (you should avoid using this unless you know
    // what you are doing).
    RHICommandList& GetRHICommandList();

private:
    std::optional<RHIDrawResources> PrepareDrawCall(const DrawDescription& drawDesc,
                                                    const DrawResourceBinding& drawBindings,
                                                    std::optional<ConstantBinding> constants);
    void SetVertexBuffers(u32 vertexBuffersFirstSlot, std::span<BufferBinding> vertexBuffers);
    void SetIndexBuffer(std::optional<BufferBinding> indexBuffer);

    GfxBackend* backend;
    NonNullPtr<RHICommandList> cmdList;

    // Used to avoid resetting the same state multiple times which can be costly on certain hardware.
    // In general draws and dispatches are recommended to be grouped by PSO, so this caching can be very efficient
    // versus binding everything each time.
    std::optional<GraphicsPipelineStateKey> cachedGraphicsPSOKey;
    std::optional<ComputePipelineStateKey> cachedComputePSOKey;
    std::optional<InputAssembly> cachedInputAssembly;
};

template <class T>
void CommandContext::EnqueueDataUpload(const Texture& texture, const T& data)
{
    EnqueueDataUpload(texture, std::span{ reinterpret_cast<const u8*>(&data), sizeof(T) });
}

template <class T>
void CommandContext::EnqueueDataUpload(const Texture& texture,
                                       const T& data,
                                       const TextureSubresource& subresource,
                                       const TextureExtent& extent)
{
    EnqueueDataUpload(texture, std::span{ reinterpret_cast<const u8*>(&data), sizeof(T) }, subresource, extent);
}

template <class T>
void CommandContext::EnqueueDataUpload(const Buffer& buffer, const T& data)
{
    EnqueueDataUpload(buffer,
                      std::span{ reinterpret_cast<const u8*>(&data), sizeof(T) },
                      BufferSubresource{ 0, sizeof(T) });
}

} // namespace vex