#pragma once

#include <vector>

#include <Vex/Containers/Span.h>
#include <Vex/Types.h>

namespace vex
{
struct ConstantBinding;

class RHIResourceLayoutBase
{
public:
    RHIResourceLayoutBase();
    ~RHIResourceLayoutBase();
    void SetLayoutResources(const ConstantBinding& constants);

    Span<const byte> GetLocalConstantsData() const;

    u32 version = 0;

    RHIResourceLayoutBase(RHIResourceLayoutBase&&) = default;
    RHIResourceLayoutBase& operator=(RHIResourceLayoutBase&&) = default;

protected:
    bool isDirty = true;

    u32 maxLocalConstantsByteSize;

    // Constant data remains always allocated, avoiding reallocations on successive draw calls.
    std::vector<byte> localConstantsData;
};

} // namespace vex