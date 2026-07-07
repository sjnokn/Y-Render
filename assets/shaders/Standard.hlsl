#pragma pack_matrix(row_major)
#include "Lighting.hlsl"

cbuffer CameraBuffer : register(b0)
{
    float4x4 uWorld;
    float4x4 uViewProj;
    float3 uCameraPosition;
    float uTime;
};

cbuffer MaterialBuffer : register(b1)
{
    float4 uBaseColor;
    float4 uAmbientColor;
    float4 uLightDirections[4];
    float4 uLightColors[4];
    float uSpecularPower;
    float uUseTexture;
    float uUseNormalTexture;
    float uUseSpecularTexture;
    int uLightCount;
    float3 uPadding;
};

Texture2D uBaseTexture : register(t0);
SamplerState uLinearSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float4 worldPosition = mul(float4(input.position, 1.0f), uWorld);
    output.position = mul(worldPosition, uViewProj);
    output.worldPosition = worldPosition.xyz;
    output.normal = normalize(mul(float4(input.normal, 0.0f), uWorld).xyz);
    output.uv = input.uv;
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(uCameraPosition - input.worldPosition);
    float4 texel = uBaseTexture.Sample(uLinearSampler, input.uv);
    float4 albedo = lerp(uBaseColor, uBaseColor * texel, saturate(uUseTexture));
    float3 lighting = uAmbientColor.rgb;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        if (i < uLightCount)
        {
            lighting += EvaluateDirectionalLight(normal, viewDir, uLightDirections[i].xyz, uLightColors[i].rgb, uLightColors[i].a, uSpecularPower);
        }
    }

    return float4(albedo.rgb * lighting, albedo.a);
}
