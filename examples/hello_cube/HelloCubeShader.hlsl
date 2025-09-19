#include <Vex.hlsli>

struct UniformStruct
{
    float time;
    uint uvGuideTextureHandle;
};

VEX_UNIFORMS(UniformStruct, Uniforms);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(in float3 position : POSITION, in float2 uv : TEXCOORD)
{
    VSOutput vs;
    vs.uv = uv;

    float timeScale = Uniforms.time * 0.5f;
    
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

    float3 scaledPosition = mul(scaleMatrix, position);
    
    float3x3 finalRotation = mul(rotationY, rotationX);
    float3 rotatedPosition = mul(finalRotation, scaledPosition);
    
    float3 worldPosition = rotatedPosition + float3(-0.4f, 0.3f, -1);
    
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
    
    vs.position = mul(projection, float4(worldPosition, 1));
    return vs;
}

static const Texture2D<float4> UVGuideTexture = GetBindlessResource(Uniforms.uvGuideTextureHandle);

SamplerState LinearSampler;
SamplerState PointSampler;

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(UVGuideTexture.Sample(LinearSampler, input.uv).rgb, 1);
}
