#pragma once

#include <string_view>

#include <Vex/Shaders/ShaderKey.h>

namespace vex
{

static constexpr std::string_view DepthStencilReadbackEntryPoint = "DepthStencilReadbackCS";

// Candidate for #embed in C++26
static constexpr std::string_view DepthStencilReadbackSource = R"SHADER(

#include <Vex.hlsli>

struct Uniforms {
    uint2 textureOffset;
    uint rowWordCount;
    uint depthTextureHandle;
    uint stencilTextureHandle;
    uint dstBufferHandle;
};

VEX_UNIFORMS(Uniforms, UniformBuffer);

static const Texture2D<float> DepthTexture = GetBindlessResource(UniformBuffer.depthTextureHandle);
static const Texture2D<uint> StencilTexture = GetBindlessResource(UniformBuffer.stencilTextureHandle);
static const RWStructuredBuffer<uint> DestinationBuffer = GetBindlessResource(UniformBuffer.dstBufferHandle);

[numthreads(8, 8, 1)]
void DepthStencilReadbackCS(uint2 threadId : SV_DispatchThreadID)
{
    float depthValue = DepthTexture.Load(int3(UniformBuffer.textureOffset + threadId.xy, 0));
    uint stencilValue = StencilTexture.Load(int3(UniformBuffer.textureOffset + threadId.xy, 0));
    uint quatizedDepth = (uint)(depthValue * 0x00FFFFFF);

    uint outputValue = 0;
    outputValue |= (stencilValue  & 0x000000FF) << 24;
    outputValue |= (quatizedDepth & 0x00FFFFFF);
    DestinationBuffer[threadId.y * UniformBuffer.rowWordCount + threadId.x] = outputValue;
}
)SHADER";

static const ShaderKey DepthStencilReadbackShaderKey{
    .sourceCode = std::string(DepthStencilReadbackSource),
    .entryPoint = std::string(DepthStencilReadbackEntryPoint),
    .type = ShaderType::ComputeShader,
    .compiler = ShaderCompilerBackend::DXC,
};

} // namespace vex