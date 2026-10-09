module;
#include <VexShaderCompiler.h>
export module Vex.ShaderCompiler;

export import Vex;

export namespace vex
{

using sc::CompilationTarget;
using sc::HitGroupKey;
using sc::RayTracingShaderKey;
using sc::Shader;
using sc::ShaderCompiler;
using sc::ShaderCompilerBackend;
using sc::ShaderDefine;
using sc::ShaderHotReloadErrorResponse;
using sc::ShaderHotReloadErrorsCallback;
using sc::ShaderKey;
using sc::ShaderReflection;

} // namespace vex
