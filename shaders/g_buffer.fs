#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gDiffuseColor;
layout (location = 3) out float gMask;

in vec3 FragPos;
in vec4 FragDiffuseColor;
in vec3 FragTransformedNormal;
in float FragMask;

void main(){
	// store the fragment position vector in the first gbuffer texture 
	gPosition = FragPos;

	// also store the per-fragment normals into the buffer 
	gNormal = normalize(FragTransformedNormal);

	// and the diffuse per-fragment color
	gDiffuseColor = FragDiffuseColor;

	// and the per-fragment mask
	gMask = FragMask;
}
