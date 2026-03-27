#include "ExamplePaths.h"
#include <Vex.h>

#if VEX_SLANG

// ----------------------------------------------------------------
// Embedded "Baz" module source — never touches disk.
// Imports the disk-based FooUtils module.
// ----------------------------------------------------------------
static constexpr const char* BazModuleSource = R"slang(
module Baz;

import FooUtils;   // resolved from the file-system include directory
__include Baz.Sub;

/// Combines foo() and bar() into a single value.
public float baz(float x)
{
    return foo(x) + bar(x);   // 2x + 0.5x = 2.5x
}
)slang";

static constexpr const char* BazSubModuleSource = R"slang(
implementing Baz;

internal float SubBaz()
{
    return 5.1;
}

)slang";

// ----------------------------------------------------------------
// Main compute shader — imports the embedded Baz module.
// Writes baz(uv.x) for every pixel into an RW buffer.
// ----------------------------------------------------------------
static constexpr const char* MainShaderSource = R"slang(
import Baz;
import Vex;

struct Uniforms
{
    uint OutputBufferHandle;
    uint Width;
    uint Height;
};
[vk::push_constant] uniform Uniforms G;

[shader("compute")]
[numthreads(8, 8, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= G.Width || tid.y >= G.Height)
        return;

    let output = GetBindlessResource<RWStructuredBuffer<float>>(G.OutputBufferHandle);

    float2 uv    = float2(tid.xy) / float2(G.Width, G.Height);
    uint   idx   = tid.y * G.Width + tid.x;
    output[idx]  = baz(uv.x);   // result in [0, 2.5]
}
)slang";

// ----------------------------------------------------------------

struct Uniforms
{
    vex::u32 OutputBufferHandle;
    vex::u32 Width;
    vex::u32 Height;
};

static constexpr vex::u32 Width  = 256;
static constexpr vex::u32 Height = 256;

int main()
{
    // FooUtils.slang sits next to this .cpp file on disk.
    const std::filesystem::path exampleDir = ExamplesDir / "hello_slang_modules";

    vex::Graphics graphics{ vex::GraphicsCreateDesc{
        .useSwapChain              = false,
        .enableGPUDebugLayer       = !VEX_SHIPPING,
        .enableGPUBasedValidation  = !VEX_SHIPPING,
        .shaderCompilerSettings    = {
            .shaderIncludeDirectories = { exampleDir },  // where Slang finds FooUtils.slang
        },
    }};

    vex::ShaderCompileContext& compileCtx = graphics.GetShaderCompileContext();

    // Register the embedded modules in the VFS.
    compileCtx.AddVirtualFile("Baz.slang",        BazModuleSource);
    compileCtx.AddVirtualFile("MainShader.slang",  MainShaderSource);
    compileCtx.AddVirtualFile("Baz/Sub.slang", BazSubModuleSource);

    // Output buffer
    const vex::u32 pixelCount   = Width * Height;
    vex::Buffer    outputBuffer = graphics.CreateBuffer(vex::BufferDesc{
        .name           = "Simple Output",
        .byteSize       = sizeof(float) * pixelCount,
        .usage          = vex::BufferUsage::GenericBuffer | vex::BufferUsage::ReadWriteBuffer,
        .memoryLocality = vex::ResourceMemoryLocality::GPUOnly,
    });

    vex::BufferBinding outputBinding{
        .buffer        = outputBuffer,
        .usage         = vex::BufferBindingUsage::RWStructuredBuffer,
        .strideByteSize = sizeof(float),
    };
    vex::BindlessHandle outputHandle = graphics.GetBindlessHandle(outputBinding);

    vex::ShaderKey mainKey{
        .path       = "MainShader.slang",
        .entryPoint = "CSMain",
        .type       = vex::ShaderType::ComputeShader,
        .compiler   = vex::ShaderCompilerBackend::Slang,
    };

    // Single dispatch — no animation, no loop needed.
    vex::CommandContext ctx = graphics.CreateCommandContext(vex::QueueType::Compute);
    ctx.BarrierBinding(outputBinding);

    Uniforms uniforms{ outputHandle.value, Width, Height };
    ctx.Dispatch(mainKey, vex::ConstantBinding{ uniforms }, { (Width + 7u) / 8u, (Height + 7u) / 8u, 1u });

    vex::BufferReadbackContext readback = ctx.EnqueueDataReadback(outputBuffer);
    vex::SyncToken token = graphics.Submit(ctx);
    graphics.WaitForTokenOnCPU(token);

    std::vector<float> cpuPixels(pixelCount);
    readback.ReadData(vex::Span<vex::byte>{
        reinterpret_cast<vex::byte*>(cpuPixels.data()),
        cpuPixels.size() * sizeof(float)
    });

    // cpuPixels[i] == baz(uv.x) == 2.5 * uv.x
}

#else
int main() {}
#endif // VEX_SLANG