#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT float hitValue;

void main()
{
    hitValue = 1.0f;
}