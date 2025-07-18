#pragma once

#include <Vex/RHI/RHIFwd.h>
#include <Vex/ShaderKey.h>
#include <Vex/Types.h>

namespace vex
{

class ResourceBindingSet;
class GfxBackend;
struct ConstantBinding;
struct ResourceBinding;
struct Texture;
struct Buffer;

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

    void Draw(const DrawDescription& drawDesc, const ResourceBindingSet& resourceBindingSet, u32 vertexCount);

    void DrawIndexed()
    {
    }
    void DrawInstancedIndexed()
    {
    }

    void Dispatch(const ShaderKey& shader, const ResourceBindingSet& resourceBindingSet, std::array<u32, 3> groupCount);

    void Copy(const Texture& source, const Texture& destination);
    void Copy(const Buffer& source, const Buffer& destination);

private:
    GfxBackend* backend;
    RHICommandList* cmdList;
};

} // namespace vex