#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out VS_OUT {
    vec3 fragPos;
    vec3 normal;
    vec2 texCoords;
    vec4 fragPosLightSpace;
} vs_out;

uniform mat4 VP;
uniform mat4 M;
uniform mat4 matrixLightSpaceVP; 

void main()
{
    vs_out.fragPos = vec3(M * vec4(aPos, 1.0));

    mat3 normalMatrixM = transpose(inverse(mat3(M)));
    vs_out.normal = normalMatrixM * aNormal;

    vs_out.texCoords = aTexCoords;

    vs_out.fragPosLightSpace = matrixLightSpaceVP * vec4(vs_out.fragPos, 1.0);

    gl_Position = VP * vec4(vs_out.fragPos, 1.0);
}