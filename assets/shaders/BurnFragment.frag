#version 130

uniform vec3 uCameraPosition;
uniform float uTime;

in vec3 vWorldPosition;
in vec3 vNormal;
in vec4 vColor;
in vec2 vFragmentData;
in vec3 vBarycentric;

out vec4 OutColor;

void main()
{
    float age = clamp(vFragmentData.x, 0.0, 1.0);
    float seed = vFragmentData.y;
    vec3 normal = normalize(vNormal);
    if (!gl_FrontFacing)
    {
        normal = -normal;
    }

    vec3 lightDirection = normalize(vec3(-0.38, 0.82, -0.42));
    float diffuse = 0.24 + max(dot(normal, lightDirection), 0.0) * 0.76;
    vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
    float rim = pow(1.0 - abs(dot(normal, viewDirection)), 2.2);
    float hot = 1.0 - smoothstep(0.008, 0.095, age);
    float whiteCore = 1.0 - smoothstep(0.0, 0.026, age);
    float pulse = 0.82 + 0.18 * sin(uTime * 17.0 + seed * 53.0);
    float edge0 = 1.0 - smoothstep(0.012, 0.075, vBarycentric.x);
    float edge1 = 1.0 - smoothstep(0.012, 0.075, vBarycentric.y);
    float edge2 = 1.0 - smoothstep(0.012, 0.075, vBarycentric.z);
    float gate0 = step(0.70, fract(sin(seed * 137.1 + 1.7) * 43758.5453));
    float gate1 = step(0.70, fract(sin(seed * 219.7 + 7.3) * 43758.5453));
    float gate2 = step(0.70, fract(sin(seed * 311.9 + 3.1) * 43758.5453));
    float faceMask = step(0.0, min(vBarycentric.x, min(vBarycentric.y, vBarycentric.z)));
    float triangleEdge = max(edge0 * gate0, max(edge1 * gate1, edge2 * gate2)) * faceMask;

    vec3 charcoal = vColor.rgb * mix(diffuse, 0.82, age);
    vec3 orangeEmission = vec3(1.0, 0.105, 0.002) * hot *
        (0.10 + triangleEdge * (1.20 + rim * 0.48)) * pulse;
    vec3 whiteEmission = vec3(1.0, 0.86, 0.38) * whiteCore *
        triangleEdge * 1.72 * pulse;
    vec3 ashEdge = vec3(0.12, 0.105, 0.09) * rim * smoothstep(0.28, 0.72, age);
    vec3 color = charcoal + orangeEmission + whiteEmission + ashEdge;

    float alpha = vColor.a;
    if (alpha < 0.015)
    {
        discard;
    }
    OutColor = vec4(color, alpha);
}
