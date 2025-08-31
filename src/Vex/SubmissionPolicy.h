#pragma once

#include <Vex/Types.h>

namespace vex
{

enum class SubmissionPolicy : u8
{
    Immediate,      // Always submits the command context immediately.
    DeferToPresent, // Might submit the command context at swapchain present time, if it allows for better batching of
                    // command contexts. The command context might also be submitted immediately.
};

} // namespace vex