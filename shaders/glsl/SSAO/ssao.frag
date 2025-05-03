#version 450

layout (location = 0) in vec2 inUV;

layout (location = 0) out float outFragColor;

layout (binding = 0) uniform SampleBuffer
{
	vec4 kernel[64];
} sampleBuffer;
layout (binding = 1) uniform UniformBuffer
{
	mat4 projection;
    mat4 view;
    mat4 model;
    float zNear;
    float zFar;
} uniformBuffer;
layout (binding = 2) uniform ParamBuffer
{
	int presentIdx;
    int sampleNum;
    float radius;
	float bias;
	int rangeCheck;
} paramBuffer;
layout (binding = 3) uniform sampler2D deferredPos;
layout (binding = 4) uniform sampler2D deferredNormal;
layout (binding = 5) uniform sampler2D ssaoNoise;

void main()
{
	vec2 uv = vec2(inUV.x, 1-inUV.y);

	vec3 normal = normalize(texture(deferredNormal, uv).rgb * 2 - 1);
	vec3 viewPos = texture(deferredPos, uv).xyz;
	
	ivec2 texDim = textureSize(deferredPos, 0); 
	ivec2 noiseDim = textureSize(ssaoNoise, 0);
	const vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/(noiseDim.y)) * uv;  
	vec3 randomVec = texture(ssaoNoise, noiseUV).xyz;

	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(tangent, normal);
	mat3 TBN = mat3(tangent, bitangent, normal);

	float occlusion = 0.0;
	float radius = paramBuffer.radius;
	int kernelSize = paramBuffer.sampleNum;
	float bias = paramBuffer.bias;
	for(int i = 0; i < kernelSize; ++i)
	{
	    vec3 samplePos = TBN * sampleBuffer.kernel[i].xyz;
		samplePos = viewPos + samplePos * radius;

	    vec4 offset = vec4(samplePos, 1.0f); 
		offset = uniformBuffer.projection * offset;
		offset.xyz /= offset.w; 
		offset.xyz = offset.xyz * 0.5f + 0.5f;
		vec2 sampleUV = vec2(offset.x, 1.0f - offset.y);
		float sampleDepth = -texture(deferredPos, sampleUV).w;

		float rangeCheck = smoothstep(0.0f, 1.0f, radius / abs(viewPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + bias ? 1.0f : 0.0f) * (paramBuffer.rangeCheck > 0 ? rangeCheck : 1.0f);  
	}
	occlusion = 1.0 - (occlusion / float(kernelSize));
	
	outFragColor = occlusion;
}