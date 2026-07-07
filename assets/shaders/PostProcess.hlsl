cbuffer PostProcessBuffer : register(b0)
{
    float uMode;
    float uTime;
    float2 uInvResolution;
};

Texture2D uSceneColor : register(t0);
SamplerState uLinearSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.uv = input.uv;
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float4 color = uSceneColor.Sample(uLinearSampler, input.uv);

    if (uMode > 0.5f && uMode < 1.5f)
    {
        float gray = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
        color.rgb = gray.xxx;
    }
    else if (uMode > 1.5f && uMode < 2.5f)
    {
        color.rgb = 1.0f - color.rgb;
    }
    else if (uMode > 2.5f)
    {
        float2 centered = input.uv * 2.0f - 1.0f;
        float vignette = smoothstep(1.15f, 0.25f, dot(centered, centered));
        color.rgb *= vignette;
    }

    return color;
}
