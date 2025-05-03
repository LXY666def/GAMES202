#version 450

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inNormal;
layout (location = 4) in vec3 inTangent;

layout (location = 0) out vec3 outViewPos;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec3 outNormal;
layout (location = 4) out vec3 outTangent;    

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

void main()
{
    gl_Position = uniformBuffer.projection * uniformBuffer.view * uniformBuffer.model * inPos;
    mat3 mNormal = transpose(inverse(mat3(uniformBuffer.view * uniformBuffer.model)));

    outViewPos = vec3(uniformBuffer.view * uniformBuffer.model * inPos);
    outUV = inUV;
    outColor = inColor;
    outNormal = normalize(mNormal * inNormal);
    outTangent = normalize(mNormal * inTangent);
}