#version 450

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

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
	int presentIdx;
    int sampleNum;
    float radius;
    float bias;
    int rangeCheck;
} paramBuffer;
layout (binding = 2) uniform sampler2D deferredPos;
layout (binding = 3) uniform sampler2D deferredNormal;
layout (binding = 4) uniform sampler2D deferredAlbedo;
layout (binding = 5) uniform sampler2D ssaoOcclusion;
layout (binding = 6) uniform sampler2D blurTex;

void main()
{
    vec2 uv = vec2(inUV.x, 1-inUV.y);
    float ao = texture(ssaoOcclusion, uv).r;
    if (paramBuffer.presentIdx == 0) {
        outFragColor = texture(deferredPos, uv);
    } else if (paramBuffer.presentIdx == 1) {
        outFragColor = texture(deferredNormal, uv);
    } else if (paramBuffer.presentIdx == 2) {
        outFragColor = texture(deferredAlbedo, uv);
    } else if (paramBuffer.presentIdx == 3) {
        outFragColor = vec4(texture(ssaoOcclusion, uv).rrr, 1.0f);
    } else if (paramBuffer.presentIdx == 4) {
        outFragColor = texture(deferredAlbedo, uv) * texture(blurTex, uv).r;
    } else if (paramBuffer.presentIdx == 5) {
        outFragColor = vec4(texture(blurTex, uv).rrr, 1.0f);
    }
}
