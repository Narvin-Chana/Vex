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
    void SetLayoutResources(RHI& rhi,
                            ResourceCleanup& resourceCleanup,
                            const std::optional<ConstantBinding>& constants,
                            std::span<RHITextureBinding> textures,
                            std::span<RHIBufferBinding> buffers,
                            RHIDescriptorPool& descriptorPool,
                            RHIAllocator& allocator);

    void SetSamplers(std::span<TextureSampler> newSamplers);
    std::span<const TextureSampler> GetStaticSamplers() const;

    std::span<const u8> GetLocalConstantsData() const;

    u32 version = 0;

    // TODO(https://trello.com/c/K2jgp9ax): temporary until we figure out buffer/memory allocation.
    MaybeUninitialized<RHIBuffer> currentInternalConstantBuffer;

protected:
    bool isDirty = true;

    const u32 maxLocalConstantsByteSize;

    // Constant data remains always allocated, avoiding reallocations on successive draw calls.
    std::vector<u8> localConstantsData;

    std::vector<TextureSampler> samplers;
};

} // namespace vex