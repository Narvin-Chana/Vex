#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include <Vex/BuildAccelerationStructure.h>
#include <Vex/Containers/Span.h>
#include <Vex/ResourceCopy.h>
#include <Vex/ResourceReadbackContext.h>
#include <Vex/ScopedGPUEvent.h>
#include <Vex/Shaders/ShaderKey.h>
#include <Vex/TextureStateMap.h>
#include <Vex/Types.h>
#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIBarrier.h>
#include <RHI/RHIBindings.h>
#include <RHI/RHIFwd.h>
#include <RHI/RHIPipelineState.h>
#include <RHI/RHITimestampQueryPool.h>

namespace vex
{

class Graphics;
struct ConstantBinding;
struct ResourceBinding;
struct Texture;
struct Buffer;
struct TextureClearValue;
struct DrawDesc;
struct RayTracingCollection;

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
                      Span<TextureClearRect> clearRects = {});

    // Performs a draw call.
    void Draw(const DrawDesc& drawDesc,
              const DrawResourceBinding& drawBindings,
              ConstantBinding constants,
              Span<const ResourceBinding> trackedResources,
              u32 vertexCount,
              u32 instanceCount = 1,
              u32 vertexOffset = 0,
              u32 instanceOffset = 0);

    // Performs an indexed draw call.
    void DrawIndexed(const DrawDesc& drawDesc,
                     const DrawResourceBinding& drawBindings,
                     ConstantBinding constants,
                     Span<const ResourceBinding> trackedResources,
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
    void Dispatch(const ShaderKey& shader,
                  ConstantBinding constants,
                  Span<const ResourceBinding> trackedResources,
                  std::array<u32, 3> groupCount);

    // Not yet implemented
    void DispatchIndirect();

    // Dispatches a ray tracing pass.
    void TraceRays(const RayTracingCollection& rayTracingCollection,
                   ConstantBinding constants,
                   Span<const ResourceBinding> trackedResources,
                   const TraceRaysDesc& rayTracingArgs);

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
    // TODO(https://trello.com/c/LUYWkd2L): add batched tlas / blas build
    // void BuildBLAS(Span<std::pair<const AccelerationStructure&, const BLASBuildDesc&>> blasToBuild);

    // Builds a Top Level Acceleration Structure for Hardware Ray Tracing, by uploading the passed in Instances.
    void BuildTLAS(const AccelerationStructure& accelerationStructure, const TLASBuildDesc& desc);
    // TODO(https://trello.com/c/LUYWkd2L): add batched tlas / blas build
    // void BuildTLAS(Span<std::pair<const AccelerationStructure&, const TLASBuildDesc&>> tlasToBuild);

    // ---------------------------------------------------------------------------------------------------------------

    // Useful for calling native API draws when wanting to render to a specific Render Target. Allows the passed in
    // lambda to be executed in a draw scope.
    void ExecuteInDrawContext(Span<const TextureBinding> renderTargets,
                              std::optional<const TextureBinding> depthStencil,
                              Span<const ResourceBinding> trackedResources,
                              const std::function<void()>& callback);

    QueryHandle BeginTimestampQuery();
    void EndTimestampQuery(QueryHandle handle);

    // Returns an object which will scope a set of commands to label them for a external debug tool such as RenderDoc or
    // Pix.
    ScopedGPUEvent CreateScopedGPUEvent(const char* markerLabel, std::array<float, 3> color = { 1, 1, 1 });

    // ---------------------------------------------------------------------------------------------------------------
    // Advanced Operations, should be used with care!
    // ---------------------------------------------------------------------------------------------------------------

    // ---------------------------------------------------------------------------------------------------------------
    // Manual synchronization is typically unnecessary as long as you use the "tracked resources" provided by
    // Draw/Dispatch/TraceRays. In the cases it is necessary we still expose it here.

    void Transition(const Buffer& buffer, RHIBarrierAccess access);
    void Transition(const Texture& texture, RHIBarrierAccess access, const TextureSubresource& subresource = {});
    void Transition(const AccelerationStructure& as, RHIBarrierAccess access);

    // ---------------------------------------------------------------------------------------------------------------

    // Returns the RHI command list associated with this context allowing for access to the native
    // CommandList/CommandContext (you should avoid using this unless you know what you are doing).
    RHICommandList& GetRHICommandList();

private:
    void FlushBarriers();
    void EnqueueTextureBarrier(const Texture& texture,
                               const TextureSubresource& subresource,
                               RHIBarrierSync dstSync,
                               RHIBarrierAccess dstAccess,
                               RHITextureLayout dstLayout);
    void EnqueueGlobalBarrier(const RHIGlobalBarrier& globalBarrier);

    void InferResourceBarriers(RHIBarrierSync syncStage, Span<const ResourceBinding> resources);

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
                                                    ConstantBinding constants,
                                                    Span<const ResourceBinding> trackedResources);
    void CheckViewportAndScissor() const;

    void SetVertexBuffers(u32 vertexBuffersFirstSlot, Span<const BufferBinding> vertexBuffers);
    void SetIndexBuffer(BufferBinding indexBuffer);

    NonNullPtr<Graphics> graphics;
    NonNullPtr<RHICommandList> cmdList;

    // TODO(https://trello.com/c/UmVOreIj): Have one per queue and add queue transfer functionality for resources!
    std::unordered_map<TextureHandle, TextureStateMap> textureStates;

    // Temporary resources (eg: staging resources) that will be marked for destruction once this command list is
    // submitted.
    std::vector<vex::Buffer> temporaryResources;

    // Used to avoid resetting the same state multiple times which can be costly on certain hardware.
    // In general draws and dispatches are recommended to be grouped by PSO, so this caching can be very efficient
    // versus binding everything each time.
    std::optional<GraphicsPipelineStateKey> cachedGraphicsPSOKey;
    std::optional<ComputePipelineStateKey> cachedComputePSOKey;
    std::optional<InputAssembly> cachedInputAssembly;

    // TODO(https://trello.com/c/D9yX2VIS): RHIBuffer/RHITextures stored in here could be invalidated if the global RHI
    // freelists are resized in between barrier Enqueue and Flush!
    std::vector<RHIBufferBarrier> pendingBufferBarriers;
    std::vector<RHITextureBarrier> pendingTextureBarriers;
    std::vector<RHIGlobalBarrier> pendingGlobalBarriers;

    bool hasInitializedViewport = false;
    bool hasInitializedScissor = false;

    friend class Graphics;
};

} // namespace vex