#version 130

in vec3 aPosition;
in vec3 aNormal;
in vec2 aUV;
in vec4 aColor;

uniform mat4 uViewProj;
uniform float uViewportHeight;
uniform float uParticleMode;

out vec4 vParticleColor;
out float vParticleSeed;
out float vParticleAge;
out float vParticleMode;

void main()
{
    gl_Position = uViewProj * vec4(aPosition, 1.0);
    float age = clamp(aUV.y, 0.0, 1.0);
    float growth = uParticleMode > 6.5 ? mix(0.62, 1.55, age) : mix(1.15, 0.55, age);
    if (uParticleMode > 2.5 && uParticleMode < 3.5)
    {
        float sparkGrowth = mix(1.0, 0.22, age);
        float flameGrowth = mix(0.48, 1.12, smoothstep(0.0, 0.20, age)) * mix(1.0, 0.18, age);
        float ashGrowth = mix(0.48, 0.84, smoothstep(0.0, 0.18, age)) * mix(1.0, 0.38, age);
        growth = aNormal.x < 0.15
            ? sparkGrowth
            : (aNormal.x < 0.72
                ? flameGrowth
                : ashGrowth);
    }
    gl_PointSize = clamp(aUV.x * growth * uViewportHeight / max(gl_Position.w, 0.08), 1.0, 132.0);
    vParticleColor = aColor;
    vParticleSeed = aNormal.x;
    vParticleAge = age;
    vParticleMode = uParticleMode;
}
