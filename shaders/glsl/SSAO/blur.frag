#version 450

layout (location = 0) in vec2 inUV;

layout (location = 0) out float outFragColor;

layout (binding = 0) uniform ParamBuffer
{
	int presentIdx;
    int sampleNum;
    float radius;
	float bias;
	int rangeCheck;
	int blurKernelRadius;
} paramBuffer;
layout (binding = 1) uniform GaussianBuffer
{
	vec4 coe[10];
} gaussianBuffer;
layout (binding = 2) uniform sampler2D ssaoTex;

void main()
{
	vec2 uv = vec2(inUV.x, 1-inUV.y);
	vec2 inv_size = 1.0f / textureSize(ssaoTex, 0);
	int radius = paramBuffer.blurKernelRadius;
	int kernelSize = radius * 2 + 1;
	float sum = 0.0f;

	for (int x = -radius; x <= radius; ++x) {
		for (int y = -radius; y <= radius; ++y) {
			float neighbor = texture(ssaoTex, uv + vec2(x, y) * inv_size).r * gaussianBuffer.coe[(x < 0) ? -x : x].x * gaussianBuffer.coe[(y < 0) ? -y : y].x;
			sum += neighbor;
		}
	}

	outFragColor = sum;
}