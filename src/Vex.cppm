module;
#include <Vex.h>
export module Vex;

export namespace vex
{

using vex::AccelerationStructure;
using vex::AccelerationStructureBinding;
using vex::AccelerationStructureDesc;
using vex::AddressMode;
using vex::Buffer;
using vex::BufferBinding;
using vex::BufferBindingUsage;
using vex::BufferDesc;
using vex::CommandContext;
using vex::FilterMode;
using vex::GLogger;
using vex::Graphics;
using vex::GraphicsCreateDesc;
using vex::Logger;
using vex::PlatformWindow;
using vex::PlatformWindowHandle;
using vex::QueueType;
using vex::ShaderType;
using vex::ShaderView;
using vex::StaticTextureSampler;
using vex::Texture;
using vex::TextureBinding;
using vex::TextureBindingUsage;
using vex::TextureClearValue;
using vex::TextureDesc;
using vex::TextureFormat;
using vex::TextureRegion;
using vex::TextureType;
using vex::Span;

using namespace BufferUsage;
using namespace TextureUsage;
using enum LogLevel;

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