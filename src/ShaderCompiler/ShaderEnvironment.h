#pragma once

#include <vector>

namespace vex::sc
{

struct ShaderDefine;

struct ShaderEnvironment
{
    std::vector<ShaderDefine> defines;
};

} // namespace vex::sc