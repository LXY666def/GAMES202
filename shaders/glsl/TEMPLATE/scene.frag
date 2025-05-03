#version 450

layout (location = 0) in vec2 inUV;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 view;
	mat4 model;
} ubo;

layout (set = 1, binding = 0) uniform sampler2D colorMap;

layout (location = 0) out vec4 outFragColor;

void main() 
{	
	outFragColor = texture(colorMap, inUV);
}