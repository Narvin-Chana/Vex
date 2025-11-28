#pragma once

#include <optional>
#include <span>

#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/NonNullPtr.h>
#include <Vex/ScopedGPUEvent.h>
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

class Graphics;
struct ConstantBinding;
struct ResourceBinding;
struct Texture;
struct Buffer;
struct TextureClearValue;
struct DrawDesc;
struct RayTracingPassDescription;

class BufferReadbackContext
{
public:
    ~BufferReadbackContext();
    BufferReadbackContext(BufferReadbackContext&& other);
    BufferReadbackContext& operator=(BufferReadbackContext&& other);

    void ReadData(std::span<byte> outData);
    [[nodiscard]] u64 GetDataByteSize() const noexcept;

private:
    BufferReadbackContext(const Buffer& buffer, Graphics& backend);

    Buffer buffer;
    NonNullPtr<Graphics> backend;
    friend class CommandContext;
};

class TextureReadbackContext
{
public:
    ~TextureReadbackContext();
    TextureReadbackContext(TextureReadbackContext&& other);
    TextureReadbackContext& operator=(TextureReadbackContext&& other);

    void ReadData(std::span<byte> outData);
    [[nodiscard]] u64 GetDataByteSize() const noexcept;
    [[nodiscard]] TextureDesc GetSourceTextureDescription() const noexcept
    {
        return textureDesc;
    };
    [[nodiscard]] std::vector<TextureRegion> GetReadbackRegions() const noexcept
    {
        return textureRegions;
    }

private:
    TextureReadbackContext(const Buffer& buffer,
                           std::span<const TextureRegion> textureRegions,
                           const TextureDesc& textureDesc,
                           Graphics& backend);

    // Buffer contains readback data from the GPU.
    // This data is aligned according to Vex internal alignment
    Buffer buffer;
    std::vector<TextureRegion> textureRegions;
    TextureDesc textureDesc;

    NonNullPtr<Graphics> backend;
    friend class CommandContext;
};

class CommandContext
{
private:
    CommandContext(NonNullPtr<Graphics> backend,
                   NonNullPtr<RHICommandList> cmdList,
                   NonNullPtr<RHITimestampQueryPool> queryPool,
                   SubmissionPolicy submissionPolicy,
                   std::span<SyncToken> dependencies);

public:
    ~CommandContext();
    CommandContext(const CommandContext& other) = delete;
    CommandContext& operator=(const CommandContext& other) = delete;
    CommandContext(CommandContext&& other) = default;
    CommandContext& operator=(CommandContext&& other) = default;

    // Sets the viewport dimensions.
    void SetViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);
    // Sets the viewport scissor.
    void SetScissor(i32 x, i32 y, u32 width, u32 height);

    // Clears a texture, by default will use the texture's ClearColor.
    void ClearTexture(const TextureBinding& binding,
                      std::optional<TextureClearValue> textureClearValue = std::nullopt,
                      std::optional<std::array<float, 4>> clearRect = std::nullopt);

    // Performs a draw call.
    void Draw(const DrawDesc& drawDesc,
              const DrawResourceBinding& drawBindings,
              ConstantBinding constants,
              u32 vertexCount,
              u32 instanceCount = 1,
              u32 vertexOffset = 0,
              u32 instanceOffset = 0);

    // Performs an indexed draw call.
    void DrawIndexed(const DrawDesc& drawDesc,
                     const DrawResourceBinding& drawBindings,
                     ConstantBinding constants,
                     u32 indexCount,
                     u32 instanceCount = 1,
                     u32 indexOffset = 0,
                     u32 vertexOffset = 0,
                     u32 instanceOffset = 0);

    // Dispatches a compute shader.
    void Dispatch(const ShaderKey& shader,
                  ConstantBinding constants,
                  std::array<u32, 3> groupCount);

    // Dispatches a ray tracing pass.
    void TraceRays(const RayTracingPassDescription& rayTracingPassDescription,
                   ConstantBinding constants,
                   std::array<u32, 3> widthHeightDepth);

    // Fills in all lower resolution mips with downsampled version of the source mip.
    void GenerateMips(const TextureBinding& textureBinding);

    // ---------------------------------------------------------------------------------------------------------------
    // Resource Copy - Will automatically transition the resources into the correct states.
    // ---------------------------------------------------------------------------------------------------------------

    // Copies the entirety of the source texture (all mips and array levels) to the destination texture.
    void Copy(const Texture& source, const Texture& destination);
    // Copies a region of the source texture to the destination texture.
    void Copy(const Texture& source, const Texture& destination, const TextureCopyDesc& textureCopyDescription);
    // Copies multiple regions of the source texture to the destination texture.
    void Copy(const Texture& source,
              const Texture& destination,
              std::span<const TextureCopyDesc> textureCopyDescriptions);
    // Copies the entirety of the source buffer to the destination buffer.
    void Copy(const Buffer& source, const Buffer& destination);
    // Copies the specified region from the source buffer to the destination buffer.
    void Copy(const Buffer& source, const Buffer& destination, const BufferCopyDesc& bufferCopyDescription);
    // Copies the contents of the buffer to the specified texture.
    void Copy(const Buffer& source, const Texture& destination);
    // Copies the contents of the buffer to a specified region in the texture.
    void Copy(const Buffer& source, const Texture& destination, const BufferTextureCopyDesc& bufferToTextureCopyDesc);
    // Copies the contents of the buffer to multiple specified regions in the texture.
    void Copy(const Buffer& source,
              const Texture& destination,
              std::span<const BufferTextureCopyDesc> bufferToTextureCopyDescs);
    // Copies the contents of the texture to the destination buffer.
    void Copy(const Texture& source, const Buffer& destination);
    // Copies the contents of the texture to the destination buffer as specified by the regions defined in the copy
    // descriptions.
    void Copy(const Texture& source,
              const Buffer& destination,
              std::span<const BufferTextureCopyDesc> bufferToTextureCopyDescs);

    // ---------------------------------------------------------------------------------------------------------------
    // Buffer Data Operations
    // ---------------------------------------------------------------------------------------------------------------

    // Enqueues data to be uploaded to a specific region inside the destination buffer, using a staging buffer when
    // necessary.
    void EnqueueDataUpload(const Buffer& buffer,
                           std::span<const byte> data,
                           const BufferRegion& region = BufferRegion::FullBuffer());

    // Enqueues a readback operation on the GPU and returns the buffer in which the data can be read.
    // Will automatically create a staging buffer with the appropriate size
    BufferReadbackContext EnqueueDataReadback(const Buffer& srcBuffer);

    // TODO(https://trello.com/c/TqsL7d1w): Add a way to readback a region of a buffer.

    // ---------------------------------------------------------------------------------------------------------------
    // Texture Data Operations
    // ---------------------------------------------------------------------------------------------------------------

    // Enqueues data to be uploaded to a texture, using a staging buffer when necessary.
    // This allows for multiple uploads to various regions inside the texture.
    // The uploadRegions should match the layout of the tightly packed 'data' parameter.
    // If the uploadRegions are empty, we suppose that you intend to upload to the entirety of the texture.
    void EnqueueDataUpload(const Texture& texture,
                           std::span<const byte> packedData,
                           std::span<const TextureRegion> textureRegions);
    // Enqueues data to be uploaded to a texture, using a staging buffer when necessary.
    void EnqueueDataUpload(const Texture& texture,
                           std::span<const byte> packedData,
                           const TextureRegion& textureRegion = TextureRegion::AllMips());

    // Enqueues for the entirety of a texture to be readback from the GPU to the specified output.
    // Will automatically use a staging buffer if necessary.
    TextureReadbackContext EnqueueDataReadback(const Texture& srcTexture,
                                               std::span<const TextureRegion> textureRegions);
    TextureReadbackContext EnqueueDataReadback(const Texture& srcTexture,
                                               const TextureRegion& textureRegion = TextureRegion::AllMips());

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
    SyncToken Submit();

    // Useful for calling native API draws when wanting to render to a specific Render Target.
    // Allows the passed in lambda to be executed in a draw scope.
    void ExecuteInDrawContext(std::span<const TextureBinding> renderTargets,
                              std::optional<const TextureBinding> depthStencil,
                              const std::function<void()>& callback);

    QueryHandle BeginTimestampQuery();
    void EndTimestampQuery(QueryHandle handle);

    // Returns the RHI command list associated with this context (you should avoid using this unless you know
    // what you are doing).
    RHICommandList& GetRHICommandList();

    // Returns an object which will scope a set of commands to label them for a external debug tool such as RenderDoc or
    // Pix
    ScopedGPUEvent CreateScopedGPUEvent(const char* markerLabel, std::array<float, 3> color = { 1, 1, 1 });

private:
    std::optional<RHIDrawResources> PrepareDrawCall(const DrawDesc& drawDesc,
                                                    const DrawResourceBinding& drawBindings,
                                                    ConstantBinding constants);
    void SetVertexBuffers(u32 vertexBuffersFirstSlot, std::span<BufferBinding> vertexBuffers);
    void SetIndexBuffer(std::optional<BufferBinding> indexBuffer);

    NonNullPtr<Graphics> backend;
    NonNullPtr<RHICommandList> cmdList;

    SubmissionPolicy submissionPolicy;

    // The command queue will insert these sync tokens as dependencies before submission.
    std::vector<SyncToken> dependencies;

    // Temporary resources (eg: staging resources) that will be marked for destruction once this command list is
    // submitted.
    std::vector<vex::Buffer> temporaryResources;

    // Used to avoid resetting the same state multiple times which can be costly on certain hardware.
    // In general draws and dispatches are recommended to be grouped by PSO, so this caching can be very efficient
    // versus binding everything each time.
    std::optional<GraphicsPipelineStateKey> cachedGraphicsPSOKey;
    std::optional<ComputePipelineStateKey> cachedComputePSOKey;
    std::optional<InputAssembly> cachedInputAssembly;

    bool hasSubmitted = false;

    friend class Graphics;
};

} // namespace vex