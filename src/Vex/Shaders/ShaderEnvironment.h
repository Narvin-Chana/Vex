#pragma once

#include <string>
#include <vector>

namespace vex
{

struct ShaderDefine;

struct ShaderEnvironment
{
    std::vector<ShaderDefine> defines;
};

} // namespace vex