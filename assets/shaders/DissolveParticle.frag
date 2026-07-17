#version 130

uniform float uTime;

in vec4 vParticleColor;
in float vParticleSeed;
in float vParticleAge;
in float vParticleMode;

out vec4 OutColor;

float Hash(vec2 value)
{
    return fract(sin(dot(value, vec2(127.1, 311.7))) * 43758.5453);
}

void main()
{
    vec2 point = gl_PointCoord * 2.0 - 1.0;
    float alpha;
    vec3 color = vParticleColor.rgb;
    if (vParticleMode > 2.5 && vParticleMode < 3.5)
    {
        if (vParticleSeed < 0.15)
        {
            float radius = length(point);
            alpha = 1.0 - smoothstep(0.18, 0.92, radius);
            float core = 1.0 - smoothstep(0.0, 0.42, radius);
            color = mix(color, vec3(1.0, 0.94, 0.54), core);
        }
        else if (vParticleSeed < 0.72)
        {
            // A rising, forked flame silhouette. Unlike a circular glow this
            // has a broad hot root, a swaying waist and two transparent tips.
            vec2 uv = vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y);
            float sway =
                sin(uTime * (7.0 + vParticleSeed * 3.0) + vParticleSeed * 41.0 + uv.y * 5.4) *
                (0.035 + uv.y * 0.075);
            float center = 0.5 + sway;
            float width = mix(0.38, 0.055, pow(uv.y, 0.72));
            float body = 1.0 - smoothstep(0.76, 1.04, abs(uv.x - center) / max(width, 0.01));
            float forkA = 1.0 - smoothstep(
                0.10,
                0.30,
                length((uv - vec2(center - 0.095, 0.78)) * vec2(1.8, 1.0)));
            float forkB = 1.0 - smoothstep(
                0.08,
                0.25,
                length((uv - vec2(center + 0.085, 0.67)) * vec2(2.0, 1.0)));
            float breakup = Hash(vec2(
                floor((uv.x + vParticleSeed) * 11.0),
                floor((uv.y + uTime * 0.24) * 13.0)));
            float baseFade = smoothstep(0.0, 0.09, uv.y);
            float tipFade = 1.0 - smoothstep(0.82 + breakup * 0.10, 1.0, uv.y);
            alpha = max(body, max(forkA, forkB) * 0.74) * baseFade * tipFade;
            alpha *= mix(0.72, 1.0, breakup);

            float hotCore = body *
                (1.0 - smoothstep(0.08, 0.54, uv.y)) *
                (1.0 - smoothstep(0.0, 0.48, abs(uv.x - center) / max(width, 0.01)));
            vec3 rootColor = vec3(1.0, 0.94, 0.58);
            vec3 middleColor = vec3(1.0, 0.22, 0.008);
            vec3 tipColor = vec3(0.36, 0.018, 0.001);
            color = mix(middleColor, tipColor, smoothstep(0.46, 0.94, uv.y));
            color = mix(color, rootColor, hotCore);
        }
        else
        {
            // Fine ash is a soft turbulent mote, not an opaque rectangular
            // sprite. Its alpha falls to zero well before the point boundary.
            float radius = length(point);
            float grain = Hash(vec2(
                floor((point.x + vParticleSeed) * 8.0),
                floor((point.y - uTime * 0.10) * 8.0)));
            float wobble =
                sin(point.x * 7.0 + point.y * 5.0 + vParticleSeed * 37.0 + uTime * 1.7) *
                0.055;
            alpha = 1.0 - smoothstep(0.34 + wobble, 0.91 + grain * 0.06, radius);
            alpha *= 0.72 + grain * 0.28;
            color *= 0.78 + grain * 0.34;
        }
    }
    else if (vParticleMode > 6.5)
    {
        float radius = length(point);
        float cloudNoise = Hash(floor((point + vec2(vParticleSeed, uTime * 0.025)) * 5.0));
        alpha = (1.0 - smoothstep(0.34 + cloudNoise * 0.12, 1.0, radius)) * (0.74 + cloudNoise * 0.26);
        color *= mix(0.82, 1.12, cloudNoise);
    }
    else
    {
        float angle = (vParticleSeed - 0.5) * 2.4;
        mat2 rotation = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
        vec2 ash = rotation * point;
        float shard = max(abs(ash.x) * 2.8, abs(ash.y) * 0.82);
        alpha = 1.0 - smoothstep(0.58, 1.0, shard);
        float ember = 1.0 - smoothstep(0.0, 0.72, vParticleAge);
        color += vec3(1.0, 0.16, 0.01) * ember * 0.32;
    }

    alpha *= vParticleColor.a;
    if (alpha < 0.015)
    {
        discard;
    }
    OutColor = vec4(color, alpha);
}
