#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec3 fragPos;
out vec2 texCoords;

uniform mat4 MVP;

void main()
{
    texCoords = aTexCoords;

    gl_Position = MVP * vec4(aPos, 1.0);
}