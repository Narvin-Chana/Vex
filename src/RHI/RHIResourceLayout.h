#pragma once

#include <vector>

#include <Vex/Containers/Span.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/TextureSampler.h>
#include <Vex/Types.h>
#include <Vex/Utility/MaybeUninitialized.h>

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
    void SetLayoutResources(ConstantBinding constants);

    void SetSamplers(Span<const TextureSampler> newSamplers);
    Span<const TextureSampler> GetStaticSamplers() const;

    Span<const byte> GetLocalConstantsData() const;

    u32 version = 0;

    RHIResourceLayoutBase(RHIResourceLayoutBase&&) = default;
    RHIResourceLayoutBase& operator=(RHIResourceLayoutBase&&) = default;

protected:
    bool isDirty = true;

    u32 maxLocalConstantsByteSize;

    // Constant data remains always allocated, avoiding reallocations on successive draw calls.
    std::vector<byte> localConstantsData;

    std::vector<TextureSampler> samplers;
};

} // namespace vex