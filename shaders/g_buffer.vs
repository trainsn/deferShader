#version 330 core
layout (location = 0) in vec3 aVertexPosition;
layout (location = 1) in vec3 aVertexNormal;
layout (location = 2) in vec4 aVertexColor; 
layout (location = 3) in float aVertexMask;

uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;
uniform mat3 uNMatrix;

out vec3 FragPos;
out vec4 FragDiffuseColor;
out vec3 FragTransformedNormal;
out float FragMask;

void main()
{
	FragDiffuseColor = aVertexColor;
	FragTransformedNormal = uNMatrix * aVertexNormal;
	FragMask = aVertexMask;
	vec4 worldPos = uMVMatrix * vec4( aVertexPosition, 1.0 );
	FragPos = worldPos.xyz;
    gl_Position        = uPMatrix * worldPos;
}
