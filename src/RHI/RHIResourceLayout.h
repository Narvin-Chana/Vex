#pragma once

#include <span>
#include <vector>

#include <Vex/FrameResource.h>
#include <Vex/RHIFwd.h>
#include <Vex/TextureSampler.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{
struct ConstantBinding;
struct RHITextureBinding;
struct RHIBufferBinding;

class RHIResourceLayoutBase
{
public:
    RHIResourceLayoutBase();
    void SetLayoutResources(std::span<const ConstantBinding> constants,
                            std::span<RHITextureBinding> textures,
                            std::span<RHIBufferBinding> buffers,
                            RHIDescriptorPool& descriptorPool);

    void SetSamplers(std::span<TextureSampler> newSamplers);
    std::span<const TextureSampler> GetStaticSamplers() const;

    std::span<const u8> GetLocalConstantsData() const;

    u32 version = 0;

    // TODO: temporary, until we figure out how to allocate an independent CBuffer per frame.
    FrameResource<UniqueHandle<RHIBuffer>> internalConstantBuffers;
    u32 FrameIndex = 0;

protected:
    bool isDirty = true;

    const u32 maxLocalConstantsByteSize;

    // Constant data remains always allocated, avoiding reallocations on successive draw calls.
    std::vector<u8> localConstantsData;

    std::vector<TextureSampler> samplers;
};

} // namespace vex