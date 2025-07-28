#pragma once

#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <Vex/TextureSampler.h>
#include <Vex/Types.h>

namespace vex
{

// Global constant buffer of memory.
// Should be updated infrequently (eg: once per frame).
struct GlobalConstant
{
    std::string name;
    u32 size;
    u32 slot;
    u32 space;
};

class RHIResourceLayout;

// Must be manually unregistered when no longer needed.
struct GlobalConstantHandle
{
    std::string name;
};

// Will automatically unregister the global constant upon destruction.
struct ScopedGlobalConstantHandle
{
    ScopedGlobalConstantHandle(RHIResourceLayout& globalLayout, std::string name);
    ~ScopedGlobalConstantHandle();

    std::string name;

private:
    RHIResourceLayout& globalLayout;
};

class RHIResourceLayout
{
public:
    virtual ~RHIResourceLayout() = default;

    void SetSamplers(std::span<TextureSampler> newSamplers);
    std::span<const TextureSampler> GetStaticSamplers() const;

    ScopedGlobalConstantHandle RegisterScopedGlobalConstant(GlobalConstant globalConstant);
    GlobalConstantHandle RegisterGlobalConstant(GlobalConstant globalConstant);
    void UnregisterGlobalConstant(GlobalConstantHandle globalConstantHandle);

    // Used to verify that the global constant would not make us bust the max size, slot or space imposed by our
    // graphics API.
    virtual bool ValidateGlobalConstant(const GlobalConstant& globalConstant) const;
    // Returns the max size of local constants that the graphics API supports.
    virtual u32 GetMaxLocalConstantSize() const = 0;

    // Should be updated each time the resource layout's graphics resource has changed. Allows relevant pipeline states
    // to be recompiled on the fly accordingly.
    u32 version = 0;

protected:
    bool isDirty = true;
    std::unordered_map<std::string, GlobalConstant> globalConstants;

    std::vector<TextureSampler> samplers;
};

} // namespace vex