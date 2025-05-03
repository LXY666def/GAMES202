#version 460

layout(location=0) in vec3 inSH0_2;
layout(location=1) in vec3 inSH3_5;
layout(location=2) in vec3 inSH6_8;
layout(location=3) in vec2 inUV;
layout(location=4) in vec3 inPos;

layout (location = 0) out vec4 outFragColor;

layout(set=0, binding=0) uniform UBO
{
	mat4 projection;
    mat4 view;
    mat4 model;
    vec4 lightPos;
    ivec4 settings;
} ubo;
layout(set=0, binding=1) uniform Envmap
{
	mat4 r;
	mat4 g;
	mat4 b;
} envmap;

layout (set = 1, binding = 0) uniform sampler2D colorMap;

void main()
{
	vec4 albedo = texture(colorMap, inUV);
	albedo = vec4(1.0f);
	vec4 factor = vec4(1.0f);
	factor.r *= dot(inSH0_2, envmap.r[0].rgb) + dot(inSH3_5, envmap.r[1].rgb) + dot(inSH6_8, envmap.r[2].rgb);
	factor.g *= dot(inSH0_2, envmap.g[0].rgb) + dot(inSH3_5, envmap.g[1].rgb) + dot(inSH6_8, envmap.g[2].rgb);
	factor.b *= dot(inSH0_2, envmap.b[0].rgb) + dot(inSH3_5, envmap.b[1].rgb) + dot(inSH6_8, envmap.b[2].rgb);
	
	outFragColor = (ubo.settings[0] == 0) ? vec4(albedo.rgb, 1.0f) : vec4(albedo.rgb * factor.rgb, 1.0f);
}