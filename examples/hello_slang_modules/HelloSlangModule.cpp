#include "ExamplePaths.h"
#include "SlangModuleShaders.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <numeric>
#include <vector>

#include <Vex.h>

#if VEX_SLANG

struct Uniforms
{
    vex::u32 OutputBufferHandle;
    vex::u32 Width;
    vex::u32 Height;
    float Time;
};

static constexpr vex::u32 Width = 256;
static constexpr vex::u32 Height = 256;

int main()
{

    //    We pass the example directory as a shader include search path
    //    so that Slang can find MathUtils.slang and Noise.slang.
    const std::filesystem::path exampleDir = ExamplesDir / "hello_slang_modules";

    vex::Graphics graphics{ vex::GraphicsCreateDesc{
        .useSwapChain = false,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING,
        .shaderCompilerSettings = {
            .shaderIncludeDirectories = { exampleDir },
        },
    } };

    //    "Pattern" module is registered in the VFS; MathUtils and Noise
    //    Will be picked from the file system
    vex::ShaderCompileContext& compileCtx = graphics.GetShaderCompileContext();

    // Register the "Pattern" module in the VFS.
    // When `import Pattern;` appears in any shader compiled with this
    // context, Slang finds it here — no .slang file on disk required.
    compileCtx.AddVirtualFile("Pattern.slang", SlangModuleShaders::PatternModuleSource);

    const vex::u32 pixelCount = Width * Height;

    vex::Buffer outputBuffer = graphics.CreateBuffer(vex::BufferDesc{
        .name = "Pattern Output",
        .byteSize = sizeof(float) * pixelCount,
        .usage = vex::BufferUsage::GenericBuffer | vex::BufferUsage::ReadWriteBuffer,
        .memoryLocality = vex::ResourceMemoryLocality::GPUOnly,
    });

    vex::BufferBinding outputBinding{
        .buffer = outputBuffer,
        .usage = vex::BufferBindingUsage::RWStructuredBuffer,
        .strideByteSize = sizeof(float),
    };
    vex::BindlessHandle outputHandle = graphics.GetBindlessHandle(outputBinding);

    compileCtx.AddVirtualFile("MainShader.slang", SlangModuleShaders::MainShaderSource);

    vex::ShaderKey mainKey{ .path = "MainShader.slang",
                            .entryPoint = "CSMain",
                            .type = vex::ShaderType::ComputeShader,
                            .compiler = vex::ShaderCompilerBackend::Slang };

    std::vector<float> cpuPixels(pixelCount);

    for (int frame = 0; frame < 3; ++frame)
    {
        const float time = static_cast<float>(frame) * 0.5f;

        vex::CommandContext ctx = graphics.CreateCommandContext(vex::QueueType::Compute);

        // Transition the buffer to a writable state before the dispatch.
        ctx.BarrierBinding(outputBinding);

        // Build push constants from the Uniforms struct.
        Uniforms uniforms{
            .OutputBufferHandle = outputHandle.value,
            .Width = Width,
            .Height = Height,
            .Time = time,
        };

        ctx.Dispatch(mainKey, vex::ConstantBinding{ uniforms }, { (Width + 7u) / 8u, (Height + 7u) / 8u, 1u });

        // Enqueue CPU readback — internally adds a barrier + copy to a
        // staging buffer; the actual data becomes available after Submit.
        vex::BufferReadbackContext readback = ctx.EnqueueDataReadback(outputBuffer);

        vex::SyncToken token = graphics.Submit(ctx);
        graphics.WaitForTokenOnCPU(token);

        // Retrieve the data from the staging buffer into our CPU vector.
        readback.ReadData(
            vex::Span<vex::byte>{ reinterpret_cast<vex::byte*>(cpuPixels.data()), cpuPixels.size() * sizeof(float) });

        const float minVal = *std::min_element(cpuPixels.begin(), cpuPixels.end());
        const float maxVal = *std::max_element(cpuPixels.begin(), cpuPixels.end());
        const float avg = std::accumulate(cpuPixels.begin(), cpuPixels.end(), 0.0f) / static_cast<float>(pixelCount);
    }

}

#else

int main()
{
    }

#endif // VEX_SLANG
