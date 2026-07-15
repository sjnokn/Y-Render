#version 130

in vec3 aPosition;
in vec3 aNormal;
in vec2 aUV;
in vec4 aColor;

uniform mat4 uWorld;
uniform mat4 uNormalMatrix;
uniform mat4 uViewProj;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vUV;
out vec4 vColor;

void main()
{
    vec4 worldPosition = uWorld * vec4(aPosition, 1.0);
    gl_Position = uViewProj * worldPosition;
    vWorldPosition = worldPosition.xyz;
    vNormal = normalize(mat3(uNormalMatrix) * aNormal);
    vUV = aUV;
    vColor = aColor;
}
