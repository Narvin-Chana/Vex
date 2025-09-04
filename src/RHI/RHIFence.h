#pragma once

#include <Vex/Types.h>

namespace vex
{

class RHIFenceBase
{
public:
    virtual u64 GetValue() const = 0;
    virtual void WaitOnCPU(u64 value) const = 0;
    virtual void SignalOnCPU(u64 value) = 0;

    u64 nextSignalValue = 1;
};

} // namespace vex