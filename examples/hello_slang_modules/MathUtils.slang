// MathUtils.slang -- shipped as a separate .slang file on disk.
// Demonstrates: a reusable math utility module.
module MathUtils;

/// Remaps x from [inMin, inMax] into [outMin, outMax].
public float remap(float x, float inMin, float inMax, float outMin, float outMax)
{
    return outMin + (x - inMin) / (inMax - inMin) * (outMax - outMin);
}

/// Smooth Hermite interpolation (Ken Perlin's smoothstep alternative).
public float smootherstep(float e0, float e1, float x)
{
    x = clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
}

/// Rotates a 2D point by `angle` radians.
public float2 rotate2D(float2 p, float angle)
{
    float s, c;
    sincos(angle, s, c);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

/// Returns the fractional part of x (equivalent to frac/fract).
public float frac2(float x) { return x - floor(x); }

/// ---- SDF helpers ----

/// Signed distance to a circle centred at p, radius r.
public float sdCircle(float2 p, float r)
{
    return length(p) - r;
}

/// Signed distance to an axis-aligned box centred at p with half-extents b.
public float sdBox(float2 p, float2 b)
{
    float2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}
