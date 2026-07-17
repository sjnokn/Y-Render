#version 130

in vec3 aPosition;
in vec3 aNormal;
in vec2 aUV;
in vec4 aColor;
in vec3 aBarycentric;

uniform mat4 uViewProj;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec4 vColor;
out vec2 vFragmentData;
out vec3 vBarycentric;

void main()
{
    vWorldPosition = aPosition;
    vNormal = normalize(aNormal);
    vColor = aColor;
    vFragmentData = aUV;
    vBarycentric = aBarycentric;
    gl_Position = uViewProj * vec4(aPosition, 1.0);
}
