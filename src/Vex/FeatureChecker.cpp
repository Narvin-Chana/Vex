#include "FeatureChecker.h"

#include <magic_enum/magic_enum.hpp>

#include <Vex/Logger.h>

namespace vex
{

#if !VEX_SHIPPING
void FeatureChecker::DumpFeatureSupport()
{
    VEX_LOG(Info,
            "Dumping feature checker support for physical device:\n\tDevice name: {}\n\tMesh Shaders: "
            "{}\n\tRayTracing: {}\n\tMax "
            "feature level: {}\n\tResource binding tier: {}\n\tShader model: {}",
            GetPhysicalDeviceName(),
            IsFeatureSupported(Feature::MeshShader),
            IsFeatureSupported(Feature::RayTracing),
            magic_enum::enum_name(GetFeatureLevel()),
            magic_enum::enum_name(GetResourceBindingTier()),
            magic_enum::enum_name(GetShaderModel()));
}
#endif

} // namespace vex