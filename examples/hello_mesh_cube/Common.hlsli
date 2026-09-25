#pragma once

float3 TransformVertex(float3 vertexPosition, float time)
{
    float timeScale = time * 0.5f;

    float cosY = cos(timeScale);
    float sinY = sin(timeScale);
    float3x3 rotationY = float3x3(
        cosY,  0.0f, sinY,
        0.0f,  1.0f, 0.0f,
        -sinY, 0.0f, cosY
    );

    float cosX = cos(timeScale * 0.7f);
    float sinX = sin(timeScale * 0.7f);
    float3x3 rotationX = float3x3(
        1.0f, 0.0f,  0.0f,
        0.0f, cosX, -sinX,
        0.0f, sinX,  cosX
    );

    float scale = 0.5f;
    float3x3 scaleMatrix = float3x3(
        scale, 0, 0,
        0, scale, 0,
        0, 0, scale
    );

    float3 offsetPosition = float3(0, 0, -10);
    float3x3 translationMatrix = float3x3(
        0, 0, offsetPosition.x,
        0, 0, offsetPosition.y,
        0, 0, offsetPosition.z
    );

    float3 scaledPosition = mul(scaleMatrix, vertexPosition);

    float3x3 finalRotation = mul(rotationY, rotationX);
    float3 rotatedPosition = mul(finalRotation, scaledPosition);

    return rotatedPosition;
}

float4 ProjectVertex(float3 worldPosition)
{
    // Quick scuffed projection matrix (perspective)
    float fov = 1.57f; // ~90 degrees in radians
    float aspect = 16.0f / 9.0f; // Assumes 16:9 aspect ratio
    float near = 0.1f;
    float far = 10000.0f;

    float f = 1.0f / tan(fov * 0.5f);
    float4x4 projection = float4x4(
        f / aspect, 0,  0,                  0,
        0,          f,  0,                  0,
        0,          0,  far / (near - far), (near * far) / (near - far),
        0,          0,  -1,                 0
    );

    return mul(projection, float4(worldPosition, 1));
}

struct Vertex
{
    float3 position;
    float2 uv;
};

struct MSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 color : COLOR;
};

MSOutput ProcessVertex(Vertex vertex, float time, float3 offset, float3 color)
{
    MSOutput vsOut;
    vsOut.uv = vertex.uv;
    vsOut.position = ProjectVertex(TransformVertex(vertex.position, time) + offset);
    vsOut.color = color;
    return vsOut;
}

// 1 Face = 1 meshlet of 6 vertices and 2 triangles
#define MESHLET_COUNT 6
// Each face has 2 triangles
#define PRIMITIVES_PER_MESHLET_COUNT (12 / MESHLET_COUNT)
// Each face has 6 vertices
#define VERTICES_PER_MESHLET_COUNT (PRIMITIVES_PER_MESHLET_COUNT * 3)

struct Payload{
    float3 meshletColor[MESHLET_COUNT];
    float3 offset;
};

float3 RandomColorFromIndex(int idx) {
    float n = float(idx);
    return float3(
        frac(sin(n * 12.9898 + 1.0) * 43758.5453),
        frac(sin(n * 12.9898 + 2.0) * 43758.5453),
        frac(sin(n * 12.9898 + 3.0) * 43758.5453)
    );
}