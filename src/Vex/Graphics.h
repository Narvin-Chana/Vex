#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <Vex/AccelerationStructure.h>
#include <Vex/Containers/Span.h>
#include <Vex/Platform/PlatformWindow.h>
#include <Vex/QueueType.h>
#include <Vex/Synchronization.h>
#include <Vex/TextureSampler.h>

#include <RHI/RHIFwd.h>
#include <RHI/RHIPhysicalDevice.h>
#include <RHI/RHISwapChain.h>

#ifndef __cpp_lib_move_only_function
// Fallback for environments with incomplete C++23 support
#include <Vex/Utility/Functional/move_only_function.h>
#endif

namespace vex
{
class TextureReadbackContext;
class CommandContext;
struct RHIPhysicalDeviceBase;
struct Texture;
struct TextureBinding;
struct BufferBinding;
struct ResourceBinding;

#ifdef __cpp_lib_move_only_function
using CPUCallback = std::move_only_function<void()>;
#else
using CPUCallback = std23::move_only_function<void()>;
#endif

struct GraphicsCreateDesc
{
    PlatformWindow platformWindow;
    // Enables or disables using a swapchain. If this is disabled, all calls to "Present" are invalid.
    bool useSwapChain = true;
    SwapChainDesc swapChainDesc;

    // Clear value to use for present textures.
    TextureClearValue presentTextureClearValue{ .color = { 0, 0, 0, 0 } };

    // Enables the GPU debug layer.
    bool enableGPUDebugLayer = !VEX_SHIPPING;
    // Enables GPU-based validation. Can be very costly in terms of performance.
    bool enableGPUBasedValidation = VEX_DEBUG;

    // This specifies the device to use when desired. If unset the "best" device according to Vex will be picked
    std::optional<PhysicalDeviceInfo> specifiedDevice;
};

struct GraphicsImpl;

class Graphics
{
public:
    explicit Graphics(const GraphicsCreateDesc& desc);
    ~Graphics() = default;
    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;
    Graphics(Graphics&&) = default;
    Graphics& operator=(Graphics&&) = default;

    // Presents the current presentTexture to the swapchain. Will stall if the GPU's next backbuffer is not yet ready
    // (depends on your FrameBuffering). If you use an HDR swapchain, this will apply HDR conversions, if necessary,
    // before copying the present texture to the swapChain.
    void Present();

    // Create a CommandContext in which GPU commands can be recorded. The command context must later on be submitted to
    // the GPU by calling vex::Graphics::Submit().
    [[nodiscard]] CommandContext CreateCommandContext(QueueType queueType);

    // Creates a new texture with the specified description.
    [[nodiscard]] Texture CreateTexture(const TextureDesc& textureDesc,
                                        ResourceLifetime lifetime = ResourceLifetime::Static);

    // Destroys a texture, the handle passed in must be the one obtained from calling CreateTexture earlier.
    // Once destroyed, the handle passed in is invalid and should no longer be used.
    void DestroyTexture(const Texture& texture);

    // Creates a new buffer with the specified description.
    [[nodiscard]] Buffer CreateBuffer(const BufferDesc& bufferDesc,
                                      ResourceLifetime lifetime = ResourceLifetime::Static);

    // Destroys a buffer, the handle passed in must be the one obtained from calling CreateBuffer earlier.
    // Once destroyed, the handle passed in is invalid and should no longer be used.
    void DestroyBuffer(const Buffer& buffer);

    // Creates an acceleration structure. Invalid for use in shaders until it is built with a CommandContext.
    [[nodiscard]] AccelerationStructure CreateAccelerationStructure(const AccelerationStructureDesc& asDesc);

    // Destroys an acceleration structure, the handle passed in must be the one obtained from calling
    // CreateAccelerationStructure earlier. Once destroyed, the handle passed in is invalid and should no longer be
    // used.
    void DestroyAccelerationStructure(const AccelerationStructure& accelerationStructure);

    // Writes data to buffer memory. This only supports buffers with ResourceMemoryLocality::CPUWrite.
    [[nodiscard]] MappedMemory MapResource(const Buffer& buffer);

    // Allows users to fetch the bindless handles for a texture binding. This bindless handle remains valid as long as
    // the resource itself is alive.
    [[nodiscard]] BindlessHandle GetBindlessHandle(const TextureBinding& bindlessResource);

    // Allows users to fetch the bindless handles for a buffer binding. This bindless handle remains valid as long as
    // the resource itself is alive.
    [[nodiscard]] BindlessHandle GetBindlessHandle(const BufferBinding& bindlessResource);

    // Allows users to fetch the bindless handles for an Acceleration Structure. This bindless handle remains valid as
    // long as the resource itself is alive.
    [[nodiscard]] BindlessHandle GetBindlessHandle(const AccelerationStructure& accelerationStructure);

    // Allows users to fetch the bindless handles for multiple resource bindings. These bindless handles remain valid as
    // long as the resources themselves are alive.
    [[nodiscard]] std::vector<BindlessHandle> GetBindlessHandles(Span<const ResourceBinding> bindlessResources);

    // Obtains the specified sampler (creating it if it doesn't yet exist) for use as a bindless sampler in a shader.
    [[nodiscard]] BindlessHandle GetBindlessSampler(const BindlessTextureSampler& sampler);

    // Sets the static samplers used by your application. Register number used in shaders must match the index in the
    // passed-in Span.
    void SetStaticSamplers(Span<const StaticTextureSampler> staticSamplers);

    // Allows you to submit the command context to the GPU, receiving a SyncToken which can be optionally used to track
    // work completion.
    SyncToken Submit(CommandContext& ctx, Span<const SyncToken> dependencies = {});

    // Allows you to submit the command contexts to the GPU, receiving a SyncToken which can be optionally used to track
    // work completion.
    std::vector<SyncToken> Submit(Span<CommandContext> commandContexts, Span<const SyncToken> dependencies = {});

    // Has the passed-in sync token been executed on the GPU yet?
    [[nodiscard]] bool IsTokenComplete(const SyncToken& token) const;

    // Have the passed-in sync tokens been executed on the GPU yet?
    [[nodiscard]] bool AreTokensComplete(Span<const SyncToken> tokens) const;

    // Waits for the passed in token to be done.
    void WaitForTokenOnCPU(const SyncToken& syncToken);

    // Flushes all currently submitted GPU commands.
    void FlushGPU();

    // Enables or disables vertical sync when presenting. Could lead to having to recreate the swapchain after the next
    // present.
    void SetUseVSync(bool useVSync);
    [[nodiscard]] bool GetUseVSync() const;

    // Determines if the swapchain is allowed to use an HDR format. Could lead to having to recreate the swapchain after
    // the next present.
    void SetUseHDRIfSupported(bool newValue);
    [[nodiscard]] bool GetUseHDRIfSupported() const;

    // Changes the preferred color space, although if unavailable Vex will fallback to other color spaces. Could lead to
    // having to recreate the swapchain after the next present.
    void SetPreferredHDRColorSpace(ColorSpace newValue);
    [[nodiscard]] ColorSpace GetPreferredHDRColorSpace() const;

    // Returns the currently used HDR color-space.
    [[nodiscard]] ColorSpace GetCurrentHDRColorSpace() const;

    // Called when the underlying window resizes, allows the swapchain to be resized.
    void OnWindowResized(u32 newWidth, u32 newHeight);
    [[nodiscard]] bool UsesSwapChain() const;

    // Obtains the current present texture handle. If the swapchain is enabled, Vex uses a present texture which is
    // copied to the backbuffer when presenting.
    [[nodiscard]] Texture GetCurrentPresentTexture();

    // Determines if the current RHI supports raytracing.
    [[nodiscard]] bool IsRayTracingSupported() const;

    // Returns Query or status if query is not yet ready
    [[nodiscard]] std::expected<Query, QueryStatus> GetTimestampValue(QueryHandle handle);

    // Returns the Vex supported physical devices
    static std::vector<PhysicalDeviceInfo> GetSupportedDevices();

private:
    std::unique_ptr<GraphicsImpl> impl;
    friend class CommandContext;
    friend struct RHIAccessor;
};

struct RHIAccessor
{
    explicit RHIAccessor(Graphics& graphics)
        : graphics{ &graphics }
    {
    }

    RHI& GetRHI() const
    {
        return graphics->rhi;
    }
    RHIDescriptorPool& GetDescriptorPool() const
    {
        return *graphics->descriptorPool;
    };

    RHITexture& GetTexture(const vex::Texture& texture) const
    {
        return graphics->GetRHITexture(texture.handle);
    }

    RHIResourceLayout& GetResourceLayout() const
    {
        return graphics->psCache->resourceLayout.value();
    }
    // Add getters if needed...
private:
    Graphics* graphics;
};

} // namespace vex