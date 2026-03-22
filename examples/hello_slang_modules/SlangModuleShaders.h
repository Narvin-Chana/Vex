#pragma once
#include "ExamplePaths.h"

#include <Vex.h>

namespace SlangModuleShaders
{

// ============================================================
// Embedded Slang module: Pattern
// ============================================================
// This module lives entirely in C++ source — it is never written to disk.
// It demonstrates the "embedded code" path: the source is registered in the
// ShaderCompileContext's Virtual File System so `import Pattern` resolves
// correctly from any shader that also uses the same context.
//
// It depends on the Noise module (on disk) which in turn depends on MathUtils.
//
inline constexpr const char* kPatternModuleSource = R"slang(
module Pattern;

import Noise;    // disk-based module: examples/hello_slang_modules/Noise.slang
import MathUtils; // disk-based module: examples/hello_slang_modules/MathUtils.slang

/// Evaluates an animated, layered procedural pattern at UV [0,1]^2.
/// Returns a greyscale intensity in [0,1].
public float evaluatePattern(float2 uv, float time)
{
    // Pan and rotate the UV space slightly over time
    uv = rotate2D(uv - 0.5, time * 0.3) + 0.5;

    // Three octaves of FBM with different offsets
    float n0 = fbm(uv * 3.0 + float2(time * 0.05, 0.0));
    float n1 = fbm(uv * 6.0 - float2(0.0, time * 0.07) + float2(5.2, 1.3));
    float n2 = fbm(uv * 12.0 + float2(n0, n1) * 1.5);

    // Domain-warped combination
    float pattern = remap(n2, 0.0, 1.0, 0.05, 0.95);

    // Add a soft SDF circle vignette in the centre
    float2 centred = uv - 0.5;
    float ring = sdCircle(centred, 0.35);
    pattern *= smootherstep(0.05, -0.05, ring); // inside = 1, outside fades

    return pattern;
}
)slang";

// ============================================================
// Embedded Slang main entry point
// ============================================================
// The main compute shader that imports the embedded Pattern module.
// It writes a 256x256 greyscale image into an RW buffer and
// computes a global average intensity for CPU readback.
//
inline constexpr const char* kMainShaderSource = R"slang(
import Pattern;
import Vex;

struct Uniforms
{
    uint OutputBufferHandle;
    uint Width;
    uint Height;
    float Time;
};
[vk::push_constant] uniform Uniforms G;

[shader("compute")]
[numthreads(8, 8, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    if (tid.x >= G.Width || tid.y >= G.Height)
        return;

    let output = GetBindlessResource<RWStructuredBuffer<float>>(G.OutputBufferHandle);

    float2 uv = float2(tid.xy) / float2(G.Width, G.Height);
    float value = evaluatePattern(uv, G.Time);

    uint idx = tid.y * G.Width + tid.x;
    output[idx] = value;
}
)slang";

} // namespace SlangModuleShaders
