#pragma once

#include <string>
#include <vector>

#include <Vex/Types.h>

namespace vex
{
class RHIBuffer;
class ResourceBindingSet;

class RHIResourceLayout;

// Must be manually unregistered when no longer needed.
struct GlobalConstantHandle
{
    std::string name;
};

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

    // Should be updated each time the resource layout's graphics resource has changed. Allows relevant pipeline states
    // to be recompiled on the fly accordingly.
    u32 version = 0;

protected:
    bool isDirty = true;
    std::vector<ResourceSampler> samplers;
};

} // namespace vex