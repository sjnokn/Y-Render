#version 130

in vec3 aPosition;
in vec2 aUV;

out vec2 vUV;

void main()
{
    gl_Position = vec4(aPosition, 1.0);
    vUV = aUV;
}
