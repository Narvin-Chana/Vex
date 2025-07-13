#pragma once

#include "Vex/Bindings.h"

#include <string>
#include <unordered_map>
#include <vector>

#include <Vex/Types.h>

namespace vex
{
class ResourceBindingSet;
}
namespace vex
{

struct ResourceSampler
{
    // TODO: implement samplers
};

class RHIResourceLayout
{
public:
    virtual ~RHIResourceLayout() = default;

    // Returns the max size of local constants that the graphics API supports.
    virtual u32 GetMaxLocalConstantSize() const = 0;
    virtual u32 GetLocalConstantsOffset() const noexcept = 0;

    virtual void Update(const ResourceBindingSet& set) = 0;

    // Should be updated each time the resource layout's graphics resource has changed. Allows relevant pipeline states
    // to be recompiled on the fly accordingly.
    u32 version = 0;

protected:
    bool isDirty = true;
    std::vector<ResourceSampler> samplers;
};

} // namespace vex