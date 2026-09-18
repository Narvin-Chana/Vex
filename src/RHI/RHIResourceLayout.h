#pragma once

#include <vector>

#include <Vex/Containers/Span.h>
#include <Vex/PipelineState.h>
#include <Vex/TextureSampler.h>
#include <Vex/Types.h>

namespace vex
{
struct ConstantBinding;

class RHIResourceLayoutBase
{
public:
    RHIResourceLayoutBase(Span<const PipelineStage> stageConstants);
    ~RHIResourceLayoutBase();
    void SetLayoutResources(Span<const std::byte> constants, Flags<PipelineStage> stageFlags);

    void SetStaticSamplers(Span<const StaticTextureSampler> newSamplers);
    [[nodiscard]] Span<const StaticTextureSampler> GetStaticSamplers() const;
    [[nodiscard]] Span<const byte> GetLocalConstantsData(u32 stageIndex) const;
    [[nodiscard]] const std::vector<PipelineStage>& GetSupportedPipelineStages() const
    {
        return supportedStages;
    }

    u32 version = 0;

    RHIResourceLayoutBase(RHIResourceLayoutBase&&) = default;
    RHIResourceLayoutBase& operator=(RHIResourceLayoutBase&&) = default;

protected:
    bool isDirty{};

    std::vector<PipelineStage> supportedStages;
    std::vector<std::vector<byte>> stageLocalConstants;

    u32 maxLocalConstantsByteSize;

    std::vector<StaticTextureSampler> staticSamplers;
};

} // namespace vex