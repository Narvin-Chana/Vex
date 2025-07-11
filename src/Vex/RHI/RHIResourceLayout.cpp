#include "RHIResourceLayout.h"

#include <ranges>

#include <Vex/Logger.h>

namespace vex
{
ScopedGlobalConstantHandle RHIResourceLayout::RegisterScopedGlobalConstant(GlobalConstant globalConstant)
{
    std::string globalContantName = globalConstant.name;
    if (globalConstants.contains(globalContantName))
    {
        VEX_LOG(Fatal, "Cannot register already registered global constant.");
    }

    if (!ValidateGlobalConstant(globalConstant))
    {
        VEX_LOG(Fatal, "Unable to validate global constant...");
    }

    globalConstants[globalContantName] = std::move(globalConstant);
    isDirty = true;

    return ScopedGlobalConstantHandle(*this, std::move(globalContantName));
}
GlobalConstantHandle RHIResourceLayout::RegisterGlobalConstant(GlobalConstant globalConstant)
{
    std::string globalContantName = globalConstant.name;
    if (globalConstants.contains(globalContantName))
    {
        VEX_LOG(Fatal, "Cannot register already registered global constant.");
    }

    if (!ValidateGlobalConstant(globalConstant))
    {
        VEX_LOG(Fatal, "Unable to validate global constant...");
    }

    globalConstants[globalContantName] = std::move(globalConstant);
    isDirty = true;

    return GlobalConstantHandle{ std::move(globalContantName) };
}

void RHIResourceLayout::UnregisterGlobalConstant(GlobalConstantHandle globalConstantHandle)
{
    if (!globalConstants.contains(globalConstantHandle.name))
    {
        VEX_LOG(Fatal, "Cannot unregister non-registered global resource handle.");
        return;
    }

    globalConstants.erase(globalConstantHandle.name);
    isDirty = true;
}

bool RHIResourceLayout::ValidateGlobalConstant(const GlobalConstant& globalConstant) const
{
    // Make sure no other global constant is using the same slot/space pair.
    for (const auto& constant : globalConstants | std::views::values)
    {
        if (constant.slot == globalConstant.slot && constant.space == globalConstant.space)
        {
            return false;
        }
    }
    return true;
}

ScopedGlobalConstantHandle::ScopedGlobalConstantHandle(RHIResourceLayout& globalLayout, std::string name)
    : name{ std::move(name) }
    , globalLayout{ globalLayout }
{
}

ScopedGlobalConstantHandle::~ScopedGlobalConstantHandle()
{
    globalLayout.UnregisterGlobalConstant({ std::move(name) });
}

} // namespace vex