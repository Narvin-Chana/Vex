#pragma once

#include <span>

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

struct DrawDescription
{
    ShaderKey vertexShader;
    ShaderKey pixelShader;
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

    void Draw(const DrawDescription& drawDesc,
              std::span<const ConstantBinding> constants,
              std::span<const ResourceBinding> reads,
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
                  std::span<const ResourceBinding> writes,
                  std::array<u32, 3> groupCount);

    void Copy(const Texture& source, const Texture& destination);

private:
    GfxBackend* backend;
    RHICommandList* cmdList;
};

} // namespace vex