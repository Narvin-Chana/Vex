module;
#include <Vex.h>
export module Vex;

export namespace vex
{

using vex::AccelerationStructure;
using vex::AccelerationStructureBinding;
using vex::AccelerationStructureDesc;
using vex::Buffer;
using vex::BufferBinding;
using vex::BufferDesc;
using vex::CommandContext;
using vex::GLogger;
using vex::Graphics;
using vex::GraphicsCreateDesc;
using vex::Logger;
using enum LogLevel;
using vex::PlatformWindow;
using vex::PlatformWindowHandle;
using vex::ShaderType;
using vex::ShaderView;
using vex::Texture;
using vex::TextureBinding;
using vex::TextureDesc;

// clang-format off
using vex::byte;
using vex::u8;
using vex::u16;
using vex::u32;
using vex::u64;
using vex::i8 ;
using vex::i16;
using vex::i32;
using vex::i64;
// clang-format on

#if VEX_SHADER_COMPILER
using sc::Shader;
using sc::ShaderCompiler;
using sc::ShaderHotReloadErrorResponse;
using sc::ShaderHotReloadErrorsCallback;
using sc::ShaderKey;
#endif

} // namespace vex