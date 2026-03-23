/**
 * hello_slang_modules — Demonstrates:
 *
 *  1. Ahead-of-Time (AOT) shader compilation via ShaderCompileContext
 *  2. Virtual File System (VFS): embedding the "Pattern" Slang module
 *     as a C++ string literal — it never touches the disk
 *  3. On-disk Slang modules (MathUtils, Noise) resolved automatically
 *     via the Slang session's search paths (the example directory)
 *  4. Module dependency chain:
 *       main shader (embedded) → Pattern (embedded VFS)
 *                                    → Noise (disk)
 *                                        → MathUtils (disk)
 *  5. GPU readback: the shader writes a 256×256 procedural pattern
 *     into a structured buffer; the CPU reads it back and prints stats
 */

#include "ExamplePaths.h"
#include "SlangModuleShaders.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <numeric>
#include <vector>

#include <Vex.h>

#if VEX_SLANG

// ----------------------------------------------------------------
// Shader push constants — must match the `Uniforms` struct in the
// embedded kMainShaderSource in SlangModuleShaders.h
// ----------------------------------------------------------------
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
    // ----------------------------------------------------------------
    // 1. Initialise Vex
    //    We pass the example directory as a shader include search path
    //    so that Slang can find MathUtils.slang and Noise.slang.
    // ----------------------------------------------------------------
    const std::filesystem::path exampleDir = ExamplesDir / "hello_slang_modules";

    vex::Graphics graphics{ vex::GraphicsCreateDesc{
        .useSwapChain = false,
        .enableGPUDebugLayer = !VEX_SHIPPING,
        .enableGPUBasedValidation = !VEX_SHIPPING,
        .shaderCompilerSettings = {
            .shaderIncludeDirectories = { exampleDir },
        },
    } };

    VEX_LOG(vex::Info, "=== hello_slang_modules ===");
    VEX_LOG(vex::Info, "Demonstrating: AOT compile context, embedded VFS modules, disk modules.");

    // ----------------------------------------------------------------
    // 2. Build the AOT ShaderCompileContext
    //
    //    This creates a persistent Slang ISession.  The embedded
    //    "Pattern" module is registered in the VFS; MathUtils and Noise
    //    are pre-loaded from disk into the same session right now so
    //    subsequent dispatches are instant (no recompilation required).
    // ----------------------------------------------------------------
    VEX_LOG(vex::Info, "");
    VEX_LOG(vex::Info, "[Step 1/3] Building AOT compile context...");

    vex::ShaderCompileContext& compileCtx = graphics.GetShaderCompileContext();

    // Register the "Pattern" module in the VFS.
    // When `import Pattern;` appears in any shader compiled with this
    // context, Slang finds it here — no .slang file on disk required.
    compileCtx.AddVirtualFile("Pattern.slang", SlangModuleShaders::kPatternModuleSource);

    // Pre-warm: load disk modules into the persistent session now.
    // If we skipped this, they'd be compiled on first dispatch instead.
    compileCtx.LoadSlangModule("MathUtils");
    compileCtx.LoadSlangModule("Noise");

    VEX_LOG(vex::Info, "  Context ready. MathUtils + Noise pre-loaded.");

    // ----------------------------------------------------------------
    // 3. Create GPU resources
    // ----------------------------------------------------------------
    VEX_LOG(vex::Info, "");
    VEX_LOG(vex::Info, "[Step 2/3] Allocating GPU resources...");

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

    VEX_LOG(vex::Info, "  Allocated {}x{} float buffer ({} KB).", Width, Height, sizeof(float) * pixelCount / 1024);

    // ----------------------------------------------------------------
    // 4. Dispatch — render three frames at different times.
    //    Only the first frame triggers shader compilation; subsequent
    //    frames reuse the compiled program from the cached context.
    // ----------------------------------------------------------------
    VEX_LOG(vex::Info, "");
    VEX_LOG(vex::Info, "[Step 3/3] Dispatching (3 frames)...");

    // Reusable ShaderKey — points at the embedded main shader source.
    vex::ShaderKey mainKey{
        .sourceCode = SlangModuleShaders::kMainShaderSource,
        .entryPoint = "CSMain",
        .type = vex::ShaderType::ComputeShader,
        .compiler = vex::ShaderCompilerBackend::Slang
    };

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

        VEX_LOG(vex::Info,
                "  Frame {}: time={:.2f}s  min={:.4f}  avg={:.4f}  max={:.4f}",
                frame,
                time,
                minVal,
                avg,
                maxVal);
    }

    VEX_LOG(vex::Info, "");
    VEX_LOG(vex::Info, "Done!");
    VEX_LOG(vex::Info, "  -> ShaderCompileContext was shared across all 3 frames.");
    VEX_LOG(vex::Info, "  -> MathUtils and Noise were compiled once (cached in the Slang session).");
    VEX_LOG(vex::Info, "  -> Pattern module lived entirely in embedded C++ string — never on disk.");
}

#else

int main()
{
    VEX_LOG(vex::Error, "This example requires Slang to be enabled (VEX_SLANG).");
}

#endif // VEX_SLANG
