#pragma once

#include <optional>
#include <span>

#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/NonNullPtr.h>
#include <Vex/Shaders/ShaderKey.h>
#include <Vex/SubmissionPolicy.h>
#include <Vex/Synchronization.h>
#include <Vex/Types.h>

#include <RHI/RHIBindings.h>
#include <RHI/RHIBuffer.h>
#include <RHI/RHICommandList.h>
#include <RHI/RHIFwd.h>
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
struct RayTracingPassDescription;

class CommandContext
{
private:
    CommandContext(NonNullPtr<GfxBackend> backend,
                   NonNullPtr<RHICommandList> cmdList,
                   SubmissionPolicy submissionPolicy,
                   std::span<SyncToken> dependencies);

public:
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

    // Fills in all mips with downsampled version of mip 0.
    void GenerateMips(const Texture& texture);

    // ---------------------------------------------------------------------------------------------------------------
    // Resource Copy - Will automatically transition the resources into the correct states.
    // ---------------------------------------------------------------------------------------------------------------

    // Copies the entirety of the source texture (all mips and array levels) to the destination texture.
    void Copy(const Texture& source, const Texture& destination);
    // Copies a region of the source texture to the destination texture.
    void Copy(const Texture& source, const Texture& destination, const TextureCopyDescription& textureCopyDescription);
    // Copies multiple regions of the source texture to the destination texture.
    void Copy(const Texture& source,
              const Texture& destination,
              std::span<const TextureCopyDescription> textureCopyDescriptions);
    // Copies the entirety of the source buffer to the destination buffer.
    void Copy(const Buffer& source, const Buffer& destination);
    // Copies the specified region from the source buffer to the destination buffer.
    void Copy(const Buffer& source, const Buffer& destination, const BufferCopyDescription& bufferCopyDescription);
    // Copies the contents of the buffer to the specified texture.
    void Copy(const Buffer& source, const Texture& destination);
    // Copies the contents of the buffer to a specified region in the texture.
    void Copy(const Buffer& source,
              const Texture& destination,
              const BufferToTextureCopyDescription& bufferToTextureCopyDescription);
    // Copies the contents of the buffer to multiple specified regions in the texture.
    void Copy(const Buffer& source,
              const Texture& destination,
              std::span<const BufferToTextureCopyDescription> bufferToTextureCopyDescriptions);

    // ---------------------------------------------------------------------------------------------------------------
    // Buffer Data Operations
    // ---------------------------------------------------------------------------------------------------------------

    // Enqueues data to be uploaded to a specific subresource inside the destination buffer, using a staging buffer when
    // necessary. Will upload data to the entirety of the destination buffer if a subresource is not specified.
    void EnqueueDataUpload(const Buffer& buffer,
                           std::span<const byte> data,
                           const std::optional<BufferSubresource>& subresource = std::nullopt);
    // Enqueues for the entirety of a buffer to be readback from the GPU to the specified output.
    // Will automatically use a staging buffer if necessary.
    void EnqueueDataReadback(const Buffer& buffer, std::span<byte> output);

    // ---------------------------------------------------------------------------------------------------------------
    // Texture Data Operations
    // ---------------------------------------------------------------------------------------------------------------

    // Enqueues data to be uploaded to a texture, using a staging buffer when necessary.
    // This allows for multiple uploads to various subresources inside the texture.
    // The uploadRegions should match the layout of the tightly packed 'data' parameter.
    // If the uploadRegions are empty, we suppose that you intend to upload to the entirety of the texture.
    void EnqueueDataUpload(const Texture& texture,
                           std::span<const byte> data,
                           std::span<const TextureUploadRegion> uploadRegions);

    // Enqueues for the entirety of a texture to be readback from the GPU to the specified output.
    // Will automatically use a staging buffer if necessary.
    void EnqueueDataReadback(const Texture& texture, std::span<byte> output);

    // ---------------------------------------------------------------------------------------------------------------

    BindlessHandle GetBindlessHandle(const ResourceBinding& resourceBinding);
    std::vector<BindlessHandle> GetBindlessHandles(std::span<const ResourceBinding> resourceBindings);

    void TransitionBindings(std::span<const ResourceBinding> resourceBindings);

    // Allows you to transition the passed in texture to the correct state. Usually this is done automatically by Vex
    // before any draws or dispatches for the resources you pass in.
    // However, in the case you are leveraging bindless resources, you are responsible for ensuring any used resources
    // are in the correct state. This contains redundancy checks so feel free to call it even if the resource is
    // potentially already in the desired state for correctness.
    void Barrier(const Texture& texture,
                 RHIBarrierSync newSync,
                 RHIBarrierAccess newAccess,
                 RHITextureLayout newLayout);

    // Allows you to transition the passed in buffer to the correct state. Usually this is done automatically by Vex
    // before any draws or dispatches for the resources you pass in.
    // However, in the case you are leveraging bindless resources, you are responsible for ensuring any used resources
    // are in the correct state. This contains redundancy checks so feel free to call it even if the resource is
    // potentially already in the desired state for correctness.
    void Barrier(const Buffer& buffer, RHIBarrierSync newSync, RHIBarrierAccess newAccess);

    // Allows you to manually submit the command context, receiving SyncTokens that allow you to later perform a CPU
    // wait for the work to be done.
    std::vector<SyncToken> Submit();

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

    NonNullPtr<GfxBackend> backend;
    NonNullPtr<RHICommandList> cmdList;

    SubmissionPolicy submissionPolicy;

    // The command queue will insert these sync tokens as dependencies before submission.
    std::vector<SyncToken> dependencies;

    // Temporary resources (eg: staging resources) that will be marked for destruction once this command list is
    // submitted.
    std::vector<ResourceCleanup::CleanupVariant> temporaryResources;

    // Used to avoid resetting the same state multiple times which can be costly on certain hardware.
    // In general draws and dispatches are recommended to be grouped by PSO, so this caching can be very efficient
    // versus binding everything each time.
    std::optional<GraphicsPipelineStateKey> cachedGraphicsPSOKey;
    std::optional<ComputePipelineStateKey> cachedComputePSOKey;
    std::optional<InputAssembly> cachedInputAssembly;

    bool hasSubmitted = false;

    friend class GfxBackend;
};

} // namespace vex