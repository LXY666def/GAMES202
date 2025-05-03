#version 450

layout (location = 0) in vec3 inPos;

layout(set=0, binding=0) uniform UBO
{
	mat4 projection;
    mat4 view;
    mat4 model;
    vec4 lightPos;
    ivec4 settings;
} ubo;

layout (location = 0) out vec3 outUVW;

void main() 
{
	outUVW = inPos;
	// outUVW.x *= -1.0;
	// Remove translation from view matrix
	mat4 viewMat = mat4(mat3(ubo.view));
	gl_Position = ubo.projection * viewMat * vec4(inPos.xyz, 1.0);
}