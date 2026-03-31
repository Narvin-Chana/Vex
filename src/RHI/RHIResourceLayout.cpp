#include "RHIResourceLayout.h"

#include <Vex/Logger.h>
#include <Vex/Bindings.h>
#include <Vex/PhysicalDevice.h>
#include <Vex/RHIImpl/RHIPhysicalDevice.h>

namespace vex
{

RHIResourceLayoutBase::RHIResourceLayoutBase()
    : maxLocalConstantsByteSize(GPhysicalDevice->GetMaxLocalConstantsByteSize())
{
    localConstantsData.reserve(maxLocalConstantsByteSize);
}

RHIResourceLayoutBase::~RHIResourceLayoutBase() = default;

void RHIResourceLayoutBase::SetLayoutResources(const ConstantBinding& constants)
{
    if (constants.IsValid())
    {
        if (constants.data.size_bytes() > maxLocalConstantsByteSize)
        {
            VEX_LOG(Fatal,
                    "Cannot pass in more bytes as local constants versus what your platform allows. You passed in {} "
                    "bytes, your graphics API allows for {} bytes.",
                    constants.data.size_bytes(),
                    maxLocalConstantsByteSize)
            return;
        }

        localConstantsData.resize(constants.data.size_bytes());
        std::memcpy(localConstantsData.data(), constants.data.data(), constants.data.size_bytes());
    }
}

Span<const byte> RHIResourceLayoutBase::GetLocalConstantsData() const
{
    return localConstantsData;
}

} // namespace vex