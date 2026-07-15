#version 130

uniform float uMode;
uniform float uTime;
uniform vec2 uInvResolution;
uniform float uBloomThreshold;
uniform float uBloomStrength;
uniform float uDepthEffectsEnabled;
uniform float uDepthFogEnabled;
uniform float uDepthFogStart;
uniform float uDepthFogEnd;
uniform float uDepthFogDensity;
uniform vec4 uDepthFogColor;
uniform float uDepthOutlineEnabled;
uniform float uDepthOutlineWidth;
uniform float uDepthOutlineStrength;
uniform vec4 uDepthOutlineColor;
uniform float uNearPlane;
uniform float uFarPlane;
uniform sampler2D uSceneColor;
uniform sampler2D uSceneDepth;
uniform sampler2D uBloomTexture;

in vec2 vUV;

out vec4 OutColor;

vec3 FreshBackground(vec2 uv)
{
    vec3 topColor = vec3(0.46, 0.76, 0.84);
    vec3 bottomColor = vec3(0.92, 0.97, 0.94);
    float vertical = smoothstep(0.0, 1.0, uv.y);
    vec3 background = mix(bottomColor, topColor, vertical);

    vec2 glowOffset = (uv - vec2(0.52, 0.72)) * vec2(1.0, 1.35);
    float glow = exp(-dot(glowOffset, glowOffset) * 3.2);
    return background + vec3(0.10, 0.08, 0.05) * glow;
}

float LinearDepth(float depth)
{
    float nearPlane = max(uNearPlane, 0.0001);
    float farPlane = max(uFarPlane, nearPlane + 0.001);
    return (nearPlane * farPlane) / max(farPlane - depth * (farPlane - nearPlane), 0.0001);
}

float DepthOutlineMask(vec2 uv, float centerDepth)
{
    if (centerDepth > 0.9995)
    {
        return 0.0;
    }

    float edge = 0.0;
    float radius = max(uDepthOutlineWidth, 0.25);
    vec2 texel = uInvResolution * radius;
    float centerLinear = LinearDepth(centerDepth);
    vec2 offsets[8];
    offsets[0] = vec2(1.0, 0.0);
    offsets[1] = vec2(-1.0, 0.0);
    offsets[2] = vec2(0.0, 1.0);
    offsets[3] = vec2(0.0, -1.0);
    offsets[4] = vec2(0.707, 0.707);
    offsets[5] = vec2(-0.707, 0.707);
    offsets[6] = vec2(0.707, -0.707);
    offsets[7] = vec2(-0.707, -0.707);
    for (int i = 0; i < 8; ++i)
    {
        float neighborDepth = texture2D(uSceneDepth, uv + offsets[i] * texel).r;
        float neighborLinear = LinearDepth(neighborDepth);
        float difference = abs(centerLinear - neighborLinear) / max(centerLinear, 0.5);
        edge = max(edge, smoothstep(0.015, 0.12, difference));
        if (neighborDepth > 0.9995)
        {
            edge = max(edge, 1.0);
        }
    }
    return clamp(edge, 0.0, 1.0);
}

void main()
{
    vec4 sceneColor = texture(uSceneColor, vUV);
    float sceneDepth = texture(uSceneDepth, vUV).r;

    // Bright pass: keep the empty background out of the bloom chain. This
    // makes the dissolve edge, rather than the light studio backdrop, glow.
    if (uMode > 3.5 && uMode < 4.5)
    {
        if (sceneDepth > 0.9995)
        {
            OutColor = vec4(0.0);
            return;
        }
        float brightness = max(max(sceneColor.r, sceneColor.g), sceneColor.b);
        float mask = smoothstep(uBloomThreshold, uBloomThreshold + 0.22, brightness);
        OutColor = vec4(max(sceneColor.rgb - vec3(uBloomThreshold), vec3(0.0)) * mask, 1.0);
        return;
    }

    // Nine-tap separable blur. The two directions are rendered into separate
    // targets by the application, keeping the pass easy to inspect in F2.
    if (uMode > 4.5 && uMode < 5.5)
    {
        vec3 blur = texture(uSceneColor, vUV).rgb * 0.227027;
        blur += texture(uSceneColor, vUV + vec2(uInvResolution.x * 1.384615, 0.0)).rgb * 0.316216;
        blur += texture(uSceneColor, vUV - vec2(uInvResolution.x * 1.384615, 0.0)).rgb * 0.316216;
        blur += texture(uSceneColor, vUV + vec2(uInvResolution.x * 3.230769, 0.0)).rgb * 0.070270;
        blur += texture(uSceneColor, vUV - vec2(uInvResolution.x * 3.230769, 0.0)).rgb * 0.070270;
        OutColor = vec4(blur, 1.0);
        return;
    }
    if (uMode > 5.5 && uMode < 6.5)
    {
        vec3 blur = texture(uSceneColor, vUV).rgb * 0.227027;
        blur += texture(uSceneColor, vUV + vec2(0.0, uInvResolution.y * 1.384615)).rgb * 0.316216;
        blur += texture(uSceneColor, vUV - vec2(0.0, uInvResolution.y * 1.384615)).rgb * 0.316216;
        blur += texture(uSceneColor, vUV + vec2(0.0, uInvResolution.y * 3.230769)).rgb * 0.070270;
        blur += texture(uSceneColor, vUV - vec2(0.0, uInvResolution.y * 3.230769)).rgb * 0.070270;
        OutColor = vec4(blur, 1.0);
        return;
    }

    if (uMode > 7.5 && uMode < 8.5)
    {
        float depth = texture2D(uSceneDepth, vUV).r;
        float distance = LinearDepth(depth);
        float depthView = depth > 0.9995 ? 1.0 : clamp(distance / 12.0, 0.0, 1.0);
        OutColor = vec4(vec3(depthView), 1.0);
        return;
    }

    vec4 color = sceneColor;
    if (sceneDepth > 0.9995)
    {
        color = vec4(FreshBackground(vUV), 1.0);
    }
    if (uMode > 6.5)
    {
        color.rgb += texture(uBloomTexture, vUV).rgb * uBloomStrength;
    }

    if (uDepthEffectsEnabled > 0.5)
    {
        float objectDepth = texture2D(uSceneDepth, vUV).r;
        if (objectDepth < 0.9995)
        {
            float distance = LinearDepth(objectDepth);
            if (uDepthFogEnabled > 0.5)
            {
                float fogStart = min(uDepthFogStart, uDepthFogEnd - 0.001);
                float fogEnd = max(uDepthFogEnd, fogStart + 0.001);
                float fog = smoothstep(fogStart, fogEnd, distance);
                fog = 1.0 - exp(-fog * max(uDepthFogDensity, 0.0) * 2.2);
                color.rgb = mix(color.rgb, uDepthFogColor.rgb, clamp(fog, 0.0, 1.0));
            }
            if (uDepthOutlineEnabled > 0.5)
            {
                float outline = DepthOutlineMask(vUV, objectDepth);
                color.rgb = mix(color.rgb, uDepthOutlineColor.rgb, outline * clamp(uDepthOutlineStrength, 0.0, 1.0));
            }
        }
    }

    if (uMode > 0.5 && uMode < 1.5)
    {
        float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
        color.rgb = vec3(gray);
    }
    else if (uMode > 1.5 && uMode < 2.5)
    {
        color.rgb = 1.0 - color.rgb;
    }
    else if (uMode > 2.5 && uMode < 3.5)
    {
        vec2 centered = vUV * 2.0 - 1.0;
        float vignette = smoothstep(1.15, 0.25, dot(centered, centered));
        color.rgb *= vignette;
    }

    OutColor = color;
}
