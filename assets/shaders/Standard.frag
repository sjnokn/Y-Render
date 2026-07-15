#version 130

uniform vec4 uBaseColor;
uniform vec4 uAmbientColor;
uniform vec3 uCameraPosition;
uniform float uTime;
uniform float uDiffuseIntensity;
uniform float uSpecularPower;
uniform float uSpecularIntensity;
uniform float uUseTexture;
uniform float uUseAlphaTest;
uniform float uAlphaCutoff;
uniform float uUseUnlit;
uniform float uShowcaseStage;
uniform float uLightingModel;
uniform float uSurfaceEffect;
uniform float uToonSteps;
uniform float uRimPower;
uniform float uRimIntensity;
uniform vec4 uRimColor;
uniform vec4 uShadowColor;
uniform vec2 uUVTiling;
uniform vec2 uUVOffset;
uniform float uDissolveAmount;
uniform float uDissolveNoiseScale;
uniform float uDissolveEdgeWidth;
uniform float uDissolveSpeed;
uniform float uDissolveEdgeIntensity;
uniform float uDissolveProgressSpeed;
uniform float uDissolveAutoProgress;
uniform vec4 uDissolveEdgeColor;
uniform int uLightCount;
uniform vec3 uLightDirections[4];
uniform vec4 uLightColors[4];
uniform sampler2D uBaseTexture;

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vUV;
in vec4 vColor;

out vec4 OutColor;

float NoiseHash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float NoiseValue(vec2 p)
{
    vec2 cell = floor(p);
    vec2 local = fract(p);
    local = local * local * (3.0 - 2.0 * local);
    float a = NoiseHash(cell);
    float b = NoiseHash(cell + vec2(1.0, 0.0));
    float c = NoiseHash(cell + vec2(0.0, 1.0));
    float d = NoiseHash(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, local.x), mix(c, d, local.x), local.y);
}

float FractalNoise(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; ++i)
    {
        value += NoiseValue(p) * amplitude;
        p = p * 2.0 + vec2(17.0, 29.0);
        amplitude *= 0.5;
    }
    return clamp(value, 0.0, 1.0);
}

void EvaluateDirectionalLight(
    vec3 normal,
    vec3 viewDir,
    vec3 lightDirection,
    vec3 lightColor,
    float intensity,
    float diffuseIntensity,
    float specularPower,
    float specularIntensity,
    out vec3 diffuse,
    out vec3 specular)
{
    vec3 lightDir = length(lightDirection) > 0.0001 ? normalize(-lightDirection) : vec3(0.0, 1.0, 0.0);
    float ndotl = max(dot(normal, lightDir), 0.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    float specularTerm = ndotl > 0.0 ? pow(max(dot(normal, halfDir), 0.0), max(specularPower, 1.0)) : 0.0;
    diffuse = lightColor * intensity * ndotl * diffuseIntensity;
    specular = lightColor * intensity * specularTerm * specularIntensity;
}

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 viewDir = normalize(uCameraPosition - vWorldPosition);
    vec2 materialUV = vUV * uUVTiling + uUVOffset;
    vec4 texel = texture(uBaseTexture, materialUV);
    vec4 materialColor = uBaseColor * vColor;
    vec4 albedo = mix(materialColor, materialColor * texel, clamp(uUseTexture, 0.0, 1.0));
    if (uUseAlphaTest > 0.5 && albedo.a < uAlphaCutoff)
    {
        discard;
    }

    float stageContact = 1.0;
    if (uShowcaseStage > 0.5)
    {
        vec2 stagePosition = vWorldPosition.xz * vec2(1.0, 1.55);
        float centerShadow = exp(-dot(stagePosition, stagePosition) * 1.8);
        float stageFade = 1.0 - smoothstep(2.3, 3.55, length(vWorldPosition.xz));
        stageContact = 1.0 - centerShadow * stageFade * 0.18;
    }

    vec3 dissolveEdgeEmission = vec3(0.0);
    vec3 dissolveEdgeTint = vec3(0.0);
    if (uSurfaceEffect > 0.5 && uSurfaceEffect < 1.5)
    {
        float dissolveScale = max(uDissolveNoiseScale, 0.01);
        vec2 noiseOffset = vec2(uTime * uDissolveSpeed, -uTime * uDissolveSpeed * 0.63);
        // Blend three world-space projections so the breakup does not look
        // stretched by a character's UV layout.
        float noiseXY = FractalNoise(vWorldPosition.xy * dissolveScale + noiseOffset);
        float noiseXZ = FractalNoise(vWorldPosition.xz * dissolveScale + noiseOffset * vec2(0.83, 1.17));
        float noiseZY = FractalNoise(vWorldPosition.zy * dissolveScale + noiseOffset * vec2(1.21, 0.71));
        float dissolveNoise = (noiseXY + noiseXZ + noiseZY) / 3.0;
        float dissolveAmount = clamp(uDissolveAmount, 0.0, 1.0);
        if (uDissolveAutoProgress > 0.5)
        {
            float cycle = fract(uTime * max(uDissolveProgressSpeed, 0.01));
            dissolveAmount = cycle < 0.5 ? cycle * 2.0 : 2.0 - cycle * 2.0;
        }
        if (dissolveNoise < dissolveAmount)
        {
            discard;
        }

        float edgeWidth = max(uDissolveEdgeWidth, 0.0001);
        float edge = 1.0 - smoothstep(dissolveAmount, min(dissolveAmount + edgeWidth, 1.0), dissolveNoise);
        float edgeCoreWidth = max(edgeWidth * 0.28, 0.0005);
        float edgeCore = 1.0 - smoothstep(dissolveAmount, min(dissolveAmount + edgeCoreWidth, 1.0), dissolveNoise);

        // A dark crust, a colored transition, and a narrow hot center make
        // the edge read as a material boundary instead of a flat neon coat.
        vec3 crustColor = uDissolveEdgeColor.rgb * 0.22;
        vec3 edgeColor = mix(crustColor, uDissolveEdgeColor.rgb, edge);
        edgeColor = mix(edgeColor, vec3(1.0), edgeCore * 0.78);
        dissolveEdgeTint = edgeColor * edge * 0.52;
        dissolveEdgeEmission = uDissolveEdgeColor.rgb * edgeCore * (uDissolveEdgeIntensity * 0.28);
    }

    if (uUseUnlit > 0.5)
    {
        OutColor = vec4(albedo.rgb * stageContact + dissolveEdgeTint + dissolveEdgeEmission, albedo.a);
        return;
    }

    if (uLightingModel > 0.5 && uLightingModel < 1.5)
    {
        vec3 toonLight = uShadowColor.rgb;
        float steps = max(uToonSteps, 1.0);
        for (int i = 0; i < 4; ++i)
        {
            if (i < uLightCount)
            {
                vec3 lightDir = length(uLightDirections[i]) > 0.0001 ? normalize(-uLightDirections[i]) : vec3(0.0, 1.0, 0.0);
                float ndotl = max(dot(normal, lightDir), 0.0);
                float band = floor(ndotl * steps) / max(steps - 1.0, 1.0);
                toonLight += band * uDiffuseIntensity * uLightColors[i].rgb * uLightColors[i].a;
            }
        }

        float rim = pow(1.0 - clamp(dot(normal, viewDir), 0.0, 1.0), max(uRimPower, 0.25)) * uRimIntensity;
        OutColor = vec4(albedo.rgb * clamp(toonLight, 0.0, 1.0) * stageContact + uRimColor.rgb * rim + dissolveEdgeTint + dissolveEdgeEmission, albedo.a);
        return;
    }

    vec3 diffuseLighting = uAmbientColor.rgb;
    vec3 specularLighting = vec3(0.0);
    for (int i = 0; i < 4; ++i)
    {
        if (i < uLightCount)
        {
            vec3 diffuse;
            vec3 specular;
            EvaluateDirectionalLight(
                normal,
                viewDir,
                uLightDirections[i],
                uLightColors[i].rgb,
                uLightColors[i].a,
                uDiffuseIntensity,
                uSpecularPower,
                uSpecularIntensity,
                diffuse,
                specular);
            diffuseLighting += diffuse;
            specularLighting += specular;
        }
    }
    OutColor = vec4((albedo.rgb * diffuseLighting + specularLighting) * stageContact + dissolveEdgeTint + dissolveEdgeEmission, albedo.a);
}
