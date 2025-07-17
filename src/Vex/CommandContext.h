#pragma once

#include <span>

#include <Vex/GraphicsPipeline.h>
#include <Vex/RHI/RHIFwd.h>
#include <Vex/ShaderKey.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

class GfxBackend;
struct ConstantBinding;
struct ResourceBinding;
struct Texture;
struct TextureClearValue;

struct DrawDescription
{
    ShaderKey vertexShader;
    ShaderKey pixelShader;
    VertexInputLayout vertexInputLayout;
    InputAssembly inputAssembly;
    RasterizerState rasterizerState;
    DepthStencilState depthStencilState;
    ColorBlendState colorBlendState;

    bool operator==(const DrawDescription& other) const = default;
};

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
    void ClearTexture(
        ResourceBinding binding,
        TextureClearValue* optionalTextureClearValue = nullptr, // Use ptr to allow for fwd declaration of type.
        std::optional<std::array<float, 4>> clearRect = std::nullopt);

    void Draw(const DrawDescription& drawDesc,
              std::span<const ConstantBinding> constants,
              std::span<const ResourceBinding> reads,
              std::span<const ResourceBinding> readWrites,
              std::span<const ResourceBinding> writes,
              u32 vertexCount);

    void DrawIndexed()
    {
    }
    void DrawInstancedIndexed()
    {
    }

    void Dispatch(const ShaderKey& shader,
                  std::span<const ConstantBinding> constants,
                  std::span<const ResourceBinding> reads,
                  std::span<const ResourceBinding> readWrites,
                  std::array<u32, 3> groupCount);

    void Copy(const Texture& source, const Texture& destination);

private:
    GfxBackend* backend;
    RHICommandList* cmdList;
};

} // namespace vex