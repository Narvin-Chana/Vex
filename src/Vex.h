#pragma once

#include <Vex/Bindings.h>
#include <Vex/CommandContext.h>
#include <Vex/DrawHelpers.h>
#include <Vex/Graphics.h>
#include <Vex/GraphicsPipeline.h>
#include <Vex/Logger.h>
#include <Vex/RHIImpl/RHIAccelerationStructure.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/RayTracing.h>
#include <Vex/TextureSampler.h>
#include <Vex/Utility/Formattable.h>
#include <Vex/Utility/NonNullPtr.h>
#include <Vex/Utility/Validation.h>

#if VEX_SHADER_COMPILER
#include <ShaderCompiler/RayTracingShaderKey.h>
#include <ShaderCompiler/Shader.h>
#include <ShaderCompiler/ShaderCompiler.h>
#include <ShaderCompiler/ShaderKey.h>
namespace vex
{
using namespace sc;
} // namespace vex
#endif