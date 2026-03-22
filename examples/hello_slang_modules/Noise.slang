// Noise.slang -- shipped as a separate .slang file on disk.
// Demonstrates: a module that imports another module (MathUtils) from disk.
module Noise;

import MathUtils; // resolved by the SlangSession's search paths (example dir)

/// Classic 2-D value noise hash.
public float hash(float2 p)
{
    p = float2(dot(p, float2(127.1, 311.7)), dot(p, float2(269.5, 183.3)));
    return frac2(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

/// Bilinear value noise in [0, 1].
public float valueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = p - i;
    float2 u = float2(smootherstep(0.0, 1.0, f.x), smootherstep(0.0, 1.0, f.y));

    float a = hash(i + float2(0.0, 0.0));
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

/// 4-octave FBM (Fractal Brownian Motion).
public float fbm(float2 p)
{
    float value  = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    [unroll(4)]
    for (int i = 0; i < 4; ++i)
    {
        value     += amplitude * valueNoise(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
        p = rotate2D(p, 0.4); // slight rotation breaks axis-alignment artefacts
    }
    return value;
}
