#pragma once

#include <span>
#include <vector>

#include <Vex/MaybeUninitialized.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/TextureSampler.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

#include <RHI/RHIFwd.h>

namespace vex
{
class ResourceCleanup;
struct ConstantBinding;
struct RHITextureBinding;
struct RHIBufferBinding;

class RHIResourceLayoutBase
{
public:
    RHIResourceLayoutBase();
    ~RHIResourceLayoutBase();
    void SetLayoutResources(const std::optional<ConstantBinding>& constants);

    void SetSamplers(std::span<TextureSampler> newSamplers);
    std::span<const TextureSampler> GetStaticSamplers() const;

    std::span<const byte> GetLocalConstantsData() const;

    u32 version = 0;

    RHIResourceLayoutBase(RHIResourceLayoutBase&&) = default;
    RHIResourceLayoutBase& operator=(RHIResourceLayoutBase&&) = default;

protected:
    bool isDirty = true;

    const u32 maxLocalConstantsByteSize;

    // Constant data remains always allocated, avoiding reallocations on successive draw calls.
    std::vector<byte> localConstantsData;

    std::vector<TextureSampler> samplers;
};

} // namespace vex