#version 330 core
// in vec3 oColor;
in vec2 oTexCoord;

out vec4 FragColor;

uniform sampler2D texture1;
uniform sampler2D texture2;

void main()
{
    FragColor = mix(texture(texture1, oTexCoord), texture(texture2, oTexCoord), 0.2);
}