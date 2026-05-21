module;
#include <Vex.h>
// Needed for access to RHI internals.
#include <Vex/RHIImpl/RHI.h>
#include <Vex/RHIImpl/RHICommandList.h>
#include <Vex/RHIImpl/RHIDescriptorPool.h>
#if VEX_DX12
#include <DX12/DX12Formats.h>
#endif
#if VEX_VULKAN
#include <Vulkan/VkFormats.h>
#endif
export module Vex;

export namespace vex
{

using QueueTypes::Count;
using vex::AABB;
using vex::AccelerationStructure;
using vex::AccelerationStructureBinding;
using vex::AccelerationStructureDesc;
using vex::AddressMode;
using vex::ASBuild;
using vex::ASGeometry;
using vex::ASGeometryType;
using vex::ASInstance;
using vex::ASType;
using vex::BindlessHandle;
using vex::BindlessTextureSampler;
using vex::BLASGeometryDesc;
using vex::Buffer;
using vex::BufferBinding;
using vex::BufferBindingUsage;
using vex::BufferCopyDesc;
using vex::BufferDesc;
using vex::BufferReadbackContext;
using vex::BufferRegion;
using vex::BufferUsage;
using vex::BufferUtil;
using vex::ByteUtil;
using vex::ColorSpace;
using vex::CommandContext;
using vex::CompareOp;
using vex::ConstantBinding;
using vex::CullMode;
using vex::DepthStencilState;
using vex::DrawDesc;
using vex::DrawResourceBinding;
using vex::FilterMode;
using vex::FrameBuffering;
using vex::FreeList;
using vex::GInvalidASHandle;
using vex::GInvalidBindlessHandle;
using vex::GInvalidBufferHandle;
using vex::GInvalidPageHandle;
using vex::GInvalidQueryHandle;
using vex::GInvalidTextureHandle;
using vex::GLogger;
using vex::Graphics;
using vex::GraphicsCreateDesc;
using vex::Handle;
using vex::Handle32;
using vex::Handle64;
using vex::InputTopology;
using vex::LogDestination;
using vex::Logger;
using vex::LogLevelToString;
using vex::NonNullPtr;
using vex::PlatformUtil;
using vex::PlatformWindow;
using vex::PlatformWindowHandle;
using vex::QueryHandle;
using vex::QueryStatus;
using vex::QueueType;
using vex::RayTracingShaderCollection;
using vex::ResourceBinding;
using vex::ResourceMemoryLocality;
using vex::RHIAccessor;
using vex::RHIBarrierAccess;
using vex::ShaderType;
using vex::ShaderView;
using vex::Span;
using vex::StaticTextureSampler;
using vex::SyncToken;
using vex::Texture;
using vex::TextureAspect;
using vex::TextureBinding;
using vex::TextureBindingUsage;
using vex::TextureClearValue;
using vex::TextureDesc;
using vex::TextureFormat;
using vex::TextureReadbackContext;
using vex::TextureRegion;
using vex::TextureSubresource;
using vex::TextureType;
using vex::TextureUsage;
using vex::TextureUtil;
using vex::TextureViewType;
using vex::TLASBuildDesc;
using vex::TLASInstanceDesc;
using vex::TraceRaysDesc;
using vex::VertexInputLayout;
using vex::Winding;

// Have to export vex::operators in order for users to get syntaxic sugar to implicitly convert a BitEnum type to Flags.
// Eg: vex::TextureUsage::ShaderRead | vex::TextureUsage::ShaderReadWrite converts to vex::Flags<vex::TextureUsage>.
using vex::Flags;
using vex::operator|;
using vex::operator&;
using vex::operator^;
using vex::operator~;

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

using vex::RHI;
using vex::RHIAccelerationStructure;
using vex::RHIAllocator;
using vex::RHICommandList;
using vex::RHICommandPool;
using vex::RHIDescriptorPool;
using vex::RHIScopedGPUEvent;
using vex::RHITimestampQueryPool;

#if VEX_SHADER_COMPILER
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
#endif

#if VEX_DX12
namespace dx12
{
using dx12::DXGIToTextureFormat;
using dx12::TextureFormatToDXGI;
} // namespace dx12
#endif

#if VEX_VULKAN
namespace vk
{
using vk::TextureFormatToVulkan;
using vk::VulkanToTextureFormat;
} // namespace vk
#endif

} // namespace vex