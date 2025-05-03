#version 460
#define PI 3.14159265358979323846

layout (set=0, binding=1) uniform sampler2D envmap;

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	vec3 dir = normalize(inUVW);
	float theta = acos(dir.y) / PI;
	float phi = atan(dir.z, dir.x) / PI * 0.5f;
	if (phi < 0){
		phi += 1.0f;
	}

	vec3 envColor = clamp(texture(envmap, vec2(phi, theta)).rgb, 0.0f, 1.0f);
	outFragColor = vec4(pow(envColor, vec3(1.0f)), 1.0f);
}