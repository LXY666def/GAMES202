#version 450

layout (location = 0) in vec3 inViewPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inNormal;
layout (location = 4) in vec3 inTangent; 

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;

layout (binding = 0) uniform UniformBuffer
{
	mat4 projection;
    mat4 view;
    mat4 model;
    float zNear;
    float zFar;
} uniformBuffer;
layout (binding = 1) uniform ParamBuffer
{
	int sampleNum;
} paramBuffer;
layout (set = 1, binding = 0) uniform sampler2D colorMap;


float LinearizeDepth(float depth)
{
	float n = uniformBuffer.zNear;
	float f = uniformBuffer.zFar;
	float z = 2 * depth-1;
	return (2.0 * n * f) / (f + n - z * (f - n));	
}

void main() 
{
	outPosition = vec4(inViewPos, LinearizeDepth(gl_FragCoord.z));
	outNormal = vec4(inNormal * 0.5f + 0.5f, 0.0f);
	outAlbedo = texture(colorMap, inUV);
}