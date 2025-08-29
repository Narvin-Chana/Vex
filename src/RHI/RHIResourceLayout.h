#pragma once

#include <span>
#include <vector>

#include <Vex/FrameResource.h>
#include <Vex/MaybeUninitialized.h>
#include <Vex/RHIFwd.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/TextureSampler.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

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

    std::span<const u8> GetLocalConstantsData() const;

    u32 version = 0;

protected:
    bool isDirty = true;

    const u32 maxLocalConstantsByteSize;

    // Constant data remains always allocated, avoiding reallocations on successive draw calls.
    std::vector<u8> localConstantsData;

    std::vector<TextureSampler> samplers;
};

} // namespace vex