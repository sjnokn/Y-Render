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
uniform float uDissolvePlaybackTime;
uniform vec4 uDissolveEdgeColor;
uniform vec4 uDissolveSecondaryColor;
uniform float uDissolveMode;
uniform vec3 uDissolveBoundsMin;
uniform vec3 uDissolveBoundsMax;
uniform vec3 uDissolveDirection;
uniform vec3 uDissolveCenter;
uniform float uDissolveNoiseInfluence;
uniform float uDissolveFlowStrength;
uniform float uDissolvePixelSize;
uniform float uDissolveProjectionMin;
uniform float uDissolveProjectionMax;
uniform int uLightCount;
uniform vec3 uLightDirections[4];
uniform vec4 uLightColors[4];
uniform sampler2D uBaseTexture;

in vec3 vWorldPosition;
in vec3 vLocalPosition;
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

float NoiseHash3D(vec3 p)
{
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float NoiseValue3D(vec3 p)
{
    vec3 cell = floor(p);
    vec3 local = fract(p);
    local = local * local * (3.0 - 2.0 * local);

    float n000 = NoiseHash3D(cell + vec3(0.0, 0.0, 0.0));
    float n100 = NoiseHash3D(cell + vec3(1.0, 0.0, 0.0));
    float n010 = NoiseHash3D(cell + vec3(0.0, 1.0, 0.0));
    float n110 = NoiseHash3D(cell + vec3(1.0, 1.0, 0.0));
    float n001 = NoiseHash3D(cell + vec3(0.0, 0.0, 1.0));
    float n101 = NoiseHash3D(cell + vec3(1.0, 0.0, 1.0));
    float n011 = NoiseHash3D(cell + vec3(0.0, 1.0, 1.0));
    float n111 = NoiseHash3D(cell + vec3(1.0, 1.0, 1.0));

    float lower = mix(mix(n000, n100, local.x), mix(n010, n110, local.x), local.y);
    float upper = mix(mix(n001, n101, local.x), mix(n011, n111, local.x), local.y);
    return mix(lower, upper, local.z);
}

float FractalNoise3D(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.55;
    for (int i = 0; i < 3; ++i)
    {
        value += NoiseValue3D(p) * amplitude;
        p = p * 2.03 + vec3(13.1, 29.7, 7.3);
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
    float dissolveScorch = 0.0;
    if (uSurfaceEffect > 0.5 && uSurfaceEffect < 1.5)
    {
        int dissolveMode = int(uDissolveMode + 0.5);
        float dissolveScale = max(uDissolveNoiseScale, 0.01);
        vec2 noiseOffset = vec2(uDissolvePlaybackTime * uDissolveSpeed, -uDissolvePlaybackTime * uDissolveSpeed * 0.63);
        vec3 boundsSize = max(uDissolveBoundsMax - uDissolveBoundsMin, vec3(0.0001));
        vec3 local01 = clamp((vLocalPosition - uDissolveBoundsMin) / boundsSize, 0.0, 1.0);
        vec3 samplePosition = vWorldPosition;

        vec2 flow = vec2(0.0);
        if (dissolveMode == 4)
        {
            vec2 flowUv = vWorldPosition.xz * 1.35;
            flow = vec2(
                FractalNoise(flowUv + vec2(0.0, uDissolvePlaybackTime * 0.21)),
                FractalNoise(flowUv + vec2(19.3, -uDissolvePlaybackTime * 0.17))) - 0.5;
            flow *= uDissolveFlowStrength;
        }

        // Three projections avoid stretching caused by a model's UV layout.
        float noiseXY = FractalNoise(samplePosition.xy * dissolveScale + noiseOffset + flow);
        float noiseXZ = FractalNoise(samplePosition.xz * dissolveScale + noiseOffset * vec2(0.83, 1.17) + flow.yx);
        float noiseZY = FractalNoise(samplePosition.zy * dissolveScale + noiseOffset * vec2(1.21, 0.71) - flow);
        float dissolveNoise = (noiseXY + noiseXZ + noiseZY) / 3.0;
        float dissolveField = dissolveNoise;

        if (dissolveMode == 5)
        {
            // Snap the actual world position to a fixed 3D grid. Every surface
            // fragment inside the same world cell therefore shares one mask
            // value and one edge color, even when the mesh rotates or scales.
            float pixelSize = max(uDissolvePixelSize, 0.005);
            vec3 pixelCell = floor(vWorldPosition / pixelSize);
            vec3 snappedWorld = (pixelCell + vec3(0.5)) * pixelSize;
            vec3 direction = length(uDissolveDirection) > 0.001 ? normalize(uDissolveDirection) : vec3(0.0, 1.0, 0.0);
            float projected = dot(snappedWorld, direction);
            float projectionRange = max(uDissolveProjectionMax - uDissolveProjectionMin, 0.0001);
            float directional = (projected - uDissolveProjectionMin) / projectionRange;

            // Coherent low-frequency 3D noise gives clusters of cells; a
            // smaller per-cell term breaks up overly smooth silhouettes.
            float cellFrequency = max(uDissolveNoiseScale, 0.05) * 0.18;
            float coherentNoise = FractalNoise3D(pixelCell * cellFrequency);
            float cellVariation = NoiseHash3D(pixelCell);
            dissolveNoise = mix(coherentNoise, cellVariation, 0.22);
            dissolveField = clamp(
                directional + (dissolveNoise - 0.5) * uDissolveNoiseInfluence,
                0.0,
                1.0);
        }
        else if (dissolveMode == 1 || dissolveMode == 6 || dissolveMode == 7)
        {
            vec3 direction = length(uDissolveDirection) > 0.001 ? normalize(uDissolveDirection) : vec3(0.0, 1.0, 0.0);
            float directionSpan = max(abs(direction.x) + abs(direction.y) + abs(direction.z), 0.001);
            float directional = dot(local01 - vec3(0.5), direction) / directionSpan + 0.5;
            dissolveField = clamp(directional + (dissolveNoise - 0.5) * uDissolveNoiseInfluence, 0.0, 1.0);
        }
        else if (dissolveMode == 2)
        {
            float sphereDistance = length(local01 - clamp(uDissolveCenter, 0.0, 1.0)) / 0.8660254;
            dissolveField = clamp(sphereDistance + (dissolveNoise - 0.5) * uDissolveNoiseInfluence, 0.0, 1.0);
        }
        else if (dissolveMode == 3)
        {
            // Incineration propagates through the model rather than randomly
            // popping isolated holes. Two noise frequencies wrinkle the front
            // into flame-like lobes while keeping one readable travel direction.
            vec3 direction = length(uDissolveDirection) > 0.001 ? normalize(uDissolveDirection) : vec3(0.0, 1.0, 0.0);
            float directionSpan = max(abs(direction.x) + abs(direction.y) + abs(direction.z), 0.001);
            float directional = dot(local01 - vec3(0.5), direction) / directionSpan + 0.5;
            float localNoiseXY = FractalNoise(vLocalPosition.xy * dissolveScale);
            float localNoiseXZ = FractalNoise(vLocalPosition.xz * dissolveScale);
            float localNoiseZY = FractalNoise(vLocalPosition.zy * dissolveScale);
            float localCoarseBurn = (localNoiseXY + localNoiseXZ + localNoiseZY) / 3.0;
            float fineBurn = FractalNoise(
                vLocalPosition.xy * dissolveScale * 2.45);
            float burnNoise = localCoarseBurn * 0.72 + fineBurn * 0.28;
            dissolveNoise = burnNoise;
            dissolveField = clamp(
                directional + (burnNoise - 0.5) * uDissolveNoiseInfluence,
                0.0,
                1.0);
        }

        float dissolveAmount = clamp(uDissolveAmount, 0.0, 1.0);
        if (uDissolveAutoProgress > 0.5)
        {
            float cycle = fract(uDissolvePlaybackTime * max(uDissolveProgressSpeed, 0.01));
            dissolveAmount = cycle < 0.5 ? cycle * 2.0 : 2.0 - cycle * 2.0;
        }

        if (dissolveField < dissolveAmount)
        {
            discard;
        }

        float edgeWidth = max(uDissolveEdgeWidth, 0.0001);
        float edgeDistance = dissolveField - dissolveAmount;
        float edge = 1.0 - smoothstep(0.0, edgeWidth, edgeDistance);
        float innerEdge = 1.0 - smoothstep(0.0, edgeWidth * 0.34, edgeDistance);
        float outerEdge = max(edge - innerEdge, 0.0);

        vec3 edgeColor = uDissolveEdgeColor.rgb;
        vec3 secondaryColor = uDissolveSecondaryColor.rgb;
        if (dissolveMode == 3)
        {
            // Reference layering, moving away from the disappearing side:
            // white-yellow heat, orange flame, red ember, then matte charcoal.
            // A second upward-scrolling field breaks every band into moving
            // tongues instead of painting a smooth red ring around the mesh.
            float flowXY = FractalNoise(
                vWorldPosition.xy * dissolveScale * vec2(1.35, 1.90) +
                vec2(dissolveNoise * 1.7, -uDissolvePlaybackTime * 1.35));
            float flowZY = FractalNoise(
                vWorldPosition.zy * dissolveScale * vec2(1.20, 1.75) +
                vec2(13.7 - dissolveNoise * 1.3, -uDissolvePlaybackTime * 1.12));
            float flowingFlame = max(flowXY, flowZY);
            float flameLick = (flowingFlame - 0.48) * edgeWidth * 0.72;
            float flowingDistance = max(edgeDistance + flameLick, 0.0);
            float flowingEdge = 1.0 - smoothstep(0.0, edgeWidth, flowingDistance);
            float activeFlame = smoothstep(0.27, 0.72, flowingFlame);

            float whiteCore =
                (1.0 - smoothstep(0.0, edgeWidth * 0.075, flowingDistance)) *
                mix(0.28, 0.86, activeFlame);
            float flameBand =
                (1.0 - smoothstep(edgeWidth * 0.075, edgeWidth * 0.34, flowingDistance)) -
                whiteCore;
            float emberBand =
                (1.0 - smoothstep(edgeWidth * 0.34, edgeWidth * 0.66, flowingDistance)) -
                whiteCore - flameBand;
            float charBand = max(flowingEdge - whiteCore - flameBand - emberBand, 0.0);
            flameBand = max(flameBand, 0.0) * mix(0.58, 1.0, activeFlame);
            emberBand = max(emberBand, 0.0);
            float flicker =
                0.60 + 0.40 * sin(
                    uDissolvePlaybackTime * 21.0 +
                    flowingFlame * 35.0 +
                    dot(vWorldPosition, vec3(6.0, 11.0, 4.0)));
            vec3 whiteHot = mix(secondaryColor, vec3(1.0, 0.97, 0.78), 0.72);
            vec3 orangeHot = mix(edgeColor, vec3(1.0, 0.31, 0.018), 0.58);
            vec3 emberRed = vec3(0.34, 0.012, 0.002);
            dissolveScorch = clamp(charBand * 2.15 + emberBand * 0.36, 0.0, 1.0);
            dissolveEdgeTint =
                whiteHot * whiteCore * 0.42 +
                orangeHot * flameBand * 0.58 +
                emberRed * emberBand * 0.46 +
                vec3(0.010, 0.002, 0.001) * charBand;
            dissolveEdgeEmission =
                whiteHot * whiteCore * uDissolveEdgeIntensity * 0.36 * flicker +
                orangeHot * flameBand * uDissolveEdgeIntensity * 0.28 * flicker +
                edgeColor * emberBand * uDissolveEdgeIntensity * 0.065;
        }
        else if (dissolveMode == 4)
        {
            float pulse = 0.72 + 0.28 * sin(uDissolvePlaybackTime * 7.0 + dissolveNoise * 18.0);
            dissolveEdgeTint = mix(edgeColor, secondaryColor, innerEdge) * edge * 0.48;
            dissolveEdgeEmission = (edgeColor * outerEdge + secondaryColor * innerEdge) *
                uDissolveEdgeIntensity * 0.34 * pulse;
        }
        else if (dissolveMode == 5)
        {
            // The reference uses whole-cell color bands, not a checkerboard:
            // white core, cyan energy, then a restrained blue-violet fringe.
            float core = 1.0 - smoothstep(0.0, edgeWidth * 0.18, edgeDistance);
            float energyBand = (1.0 - smoothstep(edgeWidth * 0.18, edgeWidth * 0.58, edgeDistance)) - core;
            float outerBand = max(edge - core - energyBand, 0.0);
            vec3 outerColor = mix(vec3(0.06, 0.10, 0.32), edgeColor, 0.28);
            dissolveEdgeTint =
                secondaryColor * core * 0.72 +
                edgeColor * energyBand * 0.60 +
                outerColor * outerBand * 0.52;
            dissolveEdgeEmission =
                secondaryColor * core * uDissolveEdgeIntensity * 0.52 +
                edgeColor * energyBand * uDissolveEdgeIntensity * 0.34 +
                outerColor * outerBand * uDissolveEdgeIntensity * 0.12;
        }
        else if (dissolveMode == 7)
        {
            dissolveEdgeTint = mix(edgeColor, secondaryColor, innerEdge) * edge * 0.42;
            dissolveEdgeEmission = secondaryColor * innerEdge * uDissolveEdgeIntensity * 0.10;
        }
        else
        {
            dissolveEdgeTint = mix(edgeColor, secondaryColor, innerEdge) * edge * 0.38;
            dissolveEdgeEmission = (edgeColor * outerEdge + secondaryColor * innerEdge) *
                uDissolveEdgeIntensity * 0.16;
        }
    }

    // A convincing burn must remove reflected albedo from the cooled band.
    // Additive edge color alone reads as neon paint, not carbonized material.
    albedo.rgb = mix(albedo.rgb, albedo.rgb * 0.035 + vec3(0.004, 0.001, 0.0005), dissolveScorch);

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
