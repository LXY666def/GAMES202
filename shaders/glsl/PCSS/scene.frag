#version 450
#define MAX_SAMPLE_NUM 64 

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inViewVec;
layout (location = 3) in vec3 inLightVec;
layout (location = 4) in vec4 inShadowCoord;
layout (location = 5) in vec3 inPosWorld;
layout (location = 6) in vec2 inUV;

layout(push_constant) uniform PushConstants {
    vec4 fov_radius_pad2;
	ivec4 block_pcf_visualize_pad;
} coe;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 view;
	mat4 model;
	mat4 lightSpace;
	vec4 lightPos;
	float zNear;
	float zFar;
} ubo;
layout (binding = 1) uniform sampler2D shadowMap;
layout (binding = 2) uniform UBO1 {
	vec4 samples[MAX_SAMPLE_NUM];
} possionSamples;

layout (set = 1, binding = 0) uniform sampler2D colorMap;

layout (location = 0) out vec4 outFragColor;

#define ambient 0.1f

// return: 1.0 if not in shadow else 0.0
float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadowMap, shadowCoord.st + off ).r;
		if ( dist < shadowCoord.z ) 
		{
			shadow = 0.0f;
		}
	}
	return shadow;
}
// return: linear depth between 0 .. 1. If u need real depth, multiply f then.
float LinearizeDepth(float depth) {
  float n = ubo.zNear;
  float f = ubo.zFar;
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n));	
}

float blockerSearchSize(vec3 posWorld) {
	vec3 light2origin = -vec3(ubo.lightPos);
	vec3 light2frag = inPosWorld - vec3(ubo.lightPos);
	float depth = dot(normalize(light2origin), light2frag);
	float r = (1.0f - ubo.zNear / depth) * coe.fov_radius_pad2[1] / (ubo.zNear * tan(coe.fov_radius_pad2[0] / 2) * 2);
	return clamp(r, 0.0, 0.5);
}

float findAvgBlockerDist(vec4 shadowCoord, float size) {
	int sampleNum = coe.block_pcf_visualize_pad[0];
	float avgBlockDist = 0.0f;
	int cnt = 0;
	for (int i = 0; i < sampleNum; i++) {
		if ( shadowCoord.z > 0.0 && shadowCoord.z < 1.0 ) {
			vec2 _sample = possionSamples.samples[i].xy * size + shadowCoord.xy;
        	float dist = texture(shadowMap, _sample).r;
			if ( dist < shadowCoord.z ) {
				avgBlockDist += LinearizeDepth(dist) * ubo.zFar;
				cnt += 1;
			}
		}
    }
	return (cnt == 0) ? LinearizeDepth(shadowCoord.z) * ubo.zFar : avgBlockDist / cnt;
}

float PCF(vec4 shadowCoord, float size) {
	int sampleNum = coe.block_pcf_visualize_pad[1];
	float cnt = 0;
	for (int i = 0; i < sampleNum; i++) {
		cnt += textureProj(shadowCoord, possionSamples.samples[i].zw * size);
    }
	return cnt / sampleNum;
}

float PCSS(vec4 shadowCoord, vec3 posWorld) {
	int visualizeType = coe.block_pcf_visualize_pad[2];
	// block search
	float searchSize = blockerSearchSize(posWorld);
	if (visualizeType == 2) {
		return searchSize;
	}
	// block size
	float avgBlockDepth = findAvgBlockerDist(shadowCoord, searchSize);
	float dReceiver = LinearizeDepth(shadowCoord.z) * ubo.zFar;
	if (visualizeType == 3) {
		return dReceiver - avgBlockDepth;
	}
	// shadow
	float penumbra = (dReceiver - avgBlockDepth) / avgBlockDepth * coe.fov_radius_pad2[1];
	penumbra = clamp(penumbra, 0.0, 0.5);
	if (visualizeType == 4) {
		return penumbra;
	}
	return PCF(shadowCoord, penumbra);
}

float filterPCF(vec4 sc)
{
	ivec2 texDim = textureSize(shadowMap, 0);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
			count++;
		}
	
	}
	return shadowFactor / count;
}

void main() 
{	
	float shadow = PCSS(inShadowCoord / inShadowCoord.w, inPosWorld);
	vec4 color = texture(colorMap, inUV);

	vec3 N = normalize(inNormal);
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 R = normalize(-reflect(L, N));
	vec3 diffuse = max(dot(N, L), ambient) * color.rgb;


	int visualizeType = coe.block_pcf_visualize_pad[2];
	float visualizeScale = coe.fov_radius_pad2[2];
	if (visualizeType == 0) {
		outFragColor = vec4(diffuse * shadow, 1.0);
	} else {
		outFragColor = vec4(vec3(shadow * visualizeScale), 1.0);
	}
}