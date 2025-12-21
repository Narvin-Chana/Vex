#pragma once

#include <optional>

#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/Containers/Span.h>
#include <Vex/ResourceReadbackContext.h>
#include <Vex/ScopedGPUEvent.h>
#include <Vex/Shaders/ShaderKey.h>
#include <Vex/Synchronization.h>
#include <Vex/Types.h>
#include <Vex/Utility/NonNullPtr.h>

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

class CommandContext
{
private:
    CommandContext(NonNullPtr<Graphics> graphics,
                   NonNullPtr<RHICommandList> cmdList,
                   NonNullPtr<RHITimestampQueryPool> queryPool);

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
                      std::span<TextureClearRect> clearRects = {});

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

    // Not yet implemented
    void DrawIndirect();

    // Not yet implemented
    void DrawIndexedIndirect();

    // Dispatches a compute shader.
    void Dispatch(const ShaderKey& shader, ConstantBinding constants, std::array<u32, 3> groupCount);

    // Not yet implemented
    void DispatchIndirect();

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
    void Copy(const Texture& source, const Texture& destination, Span<const TextureCopyDesc> textureCopyDescriptions);
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
              Span<const BufferTextureCopyDesc> bufferToTextureCopyDescs);
    // Copies the contents of the texture to the destination buffer.
    void Copy(const Texture& source, const Buffer& destination);
    // Copies the contents of the texture to the destination buffer as specified by the regions defined in the copy
    // descriptions.
    void Copy(const Texture& source,
              const Buffer& destination,
              Span<const BufferTextureCopyDesc> bufferToTextureCopyDescs);

    // ---------------------------------------------------------------------------------------------------------------
    // Buffer Data Operations
    // ---------------------------------------------------------------------------------------------------------------

    // Enqueues data to be uploaded to a specific region inside the destination buffer, using a staging buffer when
    // necessary.
    void EnqueueDataUpload(const Buffer& buffer,
                           Span<const byte> data,
                           const BufferRegion& region = BufferRegion::FullBuffer());

    // Enqueues a readback operation on the GPU and returns the buffer in which the data can be read.
    // Will automatically create a staging buffer with the appropriate size
    BufferReadbackContext EnqueueDataReadback(const Buffer& srcBuffer,
                                              const BufferRegion& region = BufferRegion::FullBuffer());

    // ---------------------------------------------------------------------------------------------------------------
    // Texture Data Operations
    // ---------------------------------------------------------------------------------------------------------------

    // Enqueues data to be uploaded to a texture, using a staging buffer when necessary.
    // This allows for multiple uploads to various regions inside the texture.
    // The uploadRegions should match the layout of the tightly packed 'data' parameter.
    // If the uploadRegions are empty, we suppose that you intend to upload to the entirety of the texture.
    void EnqueueDataUpload(const Texture& texture,
                           Span<const byte> packedData,
                           Span<const TextureRegion> textureRegions);
    // Enqueues data to be uploaded to a texture, using a staging buffer when necessary.
    void EnqueueDataUpload(const Texture& texture,
                           Span<const byte> packedData,
                           const TextureRegion& textureRegion = TextureRegion::AllMips());

    // Enqueues for the entirety of a texture to be readback from the GPU to the specified output.
    // Will automatically use a staging buffer if necessary.
    TextureReadbackContext EnqueueDataReadback(const Texture& srcTexture, Span<const TextureRegion> textureRegions);
    // Enqueues for the entirety of a texture to be readback from the GPU to the specified output.
    // Will automatically use a staging buffer if necessary.
    TextureReadbackContext EnqueueDataReadback(const Texture& srcTexture,
                                               const TextureRegion& textureRegion = TextureRegion::AllMips());

    // ---------------------------------------------------------------------------------------------------------------
    // Acceleration Structure Operations
    // ---------------------------------------------------------------------------------------------------------------

    // Builds a Bottom Level Acceleration Structure for Hardware Ray Tracing, by uploading the passed in Geometry.
    void BuildBLAS(const AccelerationStructure& accelerationStructure, const BLASBuildDesc& desc);
    //void BuildBLAS(Span<std::pair<const AccelerationStructure&, const BLASBuildDesc&>> blasToBuild);

    // Builds a Top Level Acceleration Structure for Hardware Ray Tracing, by uploading the passed in Instances.
    void BuildTLAS(const AccelerationStructure& accelerationStructure, const TLASBuildDesc& desc);
    //void BuildTLAS(Span<std::pair<const AccelerationStructure&, const TLASBuildDesc&>> tlasToBuild);

    // ---------------------------------------------------------------------------------------------------------------
    // Barrier Operations
    // ---------------------------------------------------------------------------------------------------------------
    // Vex's barrier philosophy is that they should only be applied when you have Write-After-Write (WAW) or
    // Read-After-Write (RAW) situations. If you read from a resource multiple times, then you should avoid applying
    // superfluous barriers to it.

    // Will apply a barrier to the passed in texture binding.
    void BarrierBinding(const TextureBinding& textureBinding);

    // Will apply a barrier to the passed in buffer binding.
    void BarrierBinding(const BufferBinding& bufferBinding);

    // Will apply a barrier to the passed in bindings.
    void BarrierBindings(Span<const ResourceBinding> resourceBindings);
    // Will apply a barrier to the passed in texture.
    void Barrier(const Texture& texture,
                 RHIBarrierSync newSync,
                 RHIBarrierAccess newAccess,
                 RHITextureLayout newLayout);

    // Will apply a barrier to the passed in buffer.
    void Barrier(const Buffer& buffer, RHIBarrierSync newSync, RHIBarrierAccess newAccess);

    // Will apply a barrier to the passed in acceleration structure's buffer.
    void Barrier(const AccelerationStructure& as, RHIBarrierSync newSync, RHIBarrierAccess newAccess);

    // ---------------------------------------------------------------------------------------------------------------

    // Useful for calling native API draws when wanting to render to a specific Render Target. Allows the passed in
    // lambda to be executed in a draw scope.
    void ExecuteInDrawContext(Span<const TextureBinding> renderTargets,
                              std::optional<const TextureBinding> depthStencil,
                              const std::function<void()>& callback);

    QueryHandle BeginTimestampQuery();
    void EndTimestampQuery(QueryHandle handle);

    // Returns an object which will scope a set of commands to label them for a external debug tool such as RenderDoc or
    // Pix
    ScopedGPUEvent CreateScopedGPUEvent(const char* markerLabel, std::array<float, 3> color = { 1, 1, 1 });

    // ---------------------------------------------------------------------------------------------------------------
    // Advanced Operations, should be used with care!
    // ---------------------------------------------------------------------------------------------------------------

    // Returns the RHI command list associated with this context allowing for access to the native
    // CommandList/CommandContext (you should avoid using this unless you know what you are doing).
    RHICommandList& GetRHICommandList();

    // Flushes all pending barriers. Vex automatically batches barrier submissions just before GPU operations (eg:
    // Draw/Dispatch). This means that you shouldn't call this function in most situations. This remains exposed for
    // when you have custom RHI code which can create GPU operations.
    void FlushBarriers();

private:
    // Creates a temporary staging buffer that will be destroyed once the command context is done executing.
    // Buffer creation invalidates pointers to existing RHI buffers.
    Buffer CreateTemporaryStagingBuffer(const std::string& name,
                                        u64 byteSize,
                                        BufferUsage::Flags additionalUsages = BufferUsage::None);

    // Creates a temporary buffer that will be destroyed once the command context is done executing.
    // Buffer creation invalidates pointers to existing RHI buffers.
    Buffer CreateTemporaryBuffer(const BufferDesc& desc);

    std::optional<RHIDrawResources> PrepareDrawCall(const DrawDesc& drawDesc,
                                                    const DrawResourceBinding& drawBindings,
                                                    ConstantBinding constants);
    void CheckViewportAndScissor() const;

    [[nodiscard]] std::vector<RHIBufferBarrier> SetVertexBuffers(u32 vertexBuffersFirstSlot,
                                                                 Span<const BufferBinding> vertexBuffers);
    [[nodiscard]] std::optional<RHIBufferBarrier> SetIndexBuffer(std::optional<BufferBinding> indexBuffer);

    void EnqueueBarriers(Span<const RHITextureBarrier> barriers);
    void EnqueueBarriers(Span<const RHIBufferBarrier> barriers);

    NonNullPtr<Graphics> graphics;
    NonNullPtr<RHICommandList> cmdList;

    // Temporary resources (eg: staging resources) that will be marked for destruction once this command list is
    // submitted.
    std::vector<vex::Buffer> temporaryResources;

    // Used to avoid resetting the same state multiple times which can be costly on certain hardware.
    // In general draws and dispatches are recommended to be grouped by PSO, so this caching can be very efficient
    // versus binding everything each time.
    std::optional<GraphicsPipelineStateKey> cachedGraphicsPSOKey;
    std::optional<ComputePipelineStateKey> cachedComputePSOKey;
    std::optional<InputAssembly> cachedInputAssembly;

    std::vector<RHIBufferBarrier> pendingBufferBarriers;
    std::vector<RHITextureBarrier> pendingTextureBarriers;

    bool hasInitializedViewport = false;
    bool hasInitializedScissor = false;

    friend class Graphics;
};

} // namespace vex