#version 460

layout(location=0) in vec3 inPos;
layout(location=1) in vec2 inUV;
layout(location=2) in vec3 inSH0_2;
layout(location=3) in vec3 inSH3_5;
layout(location=4) in vec3 inSH6_8;

layout(location=0) out vec3 outSH0_2;
layout(location=1) out vec3 outSH3_5;
layout(location=2) out vec3 outSH6_8;
layout(location=3) out vec2 outUV;
layout(location=4) out vec3 outPos;

layout(set=0, binding=0) uniform UBO
{
	mat4 projection;
    mat4 view;
    mat4 model;
    vec4 lightPos;
    ivec4 settings;
} ubo;

void main() {
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPos, 1.0);
    outSH0_2 = inSH0_2;
    outSH3_5 = inSH3_5;
    outSH6_8 = inSH6_8;
    outUV = inUV;
    outPos = inPos;
}