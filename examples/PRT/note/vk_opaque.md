# Vulkan光追有关Opaque的问题
被vkQueueWaitIdle处的ERROR_DEVICE_LOST硬控一晚，验证层又不报错，最后定位是有关渲染时opaque物体的设置问题。

首先明确opaque物体与shader的关系。对于标记为opaque的物体，不会进入anyhit shader，因为anyhit shader主要用于获得非opaque物体的渲染效果，控制是否记录此次intersection。因此我猜测由于我的程序没有正确设置opaque导致调用了anyhit shader，而我并没有创建anyhit shader，导致device无法找到anyhit shader，最终超时报出ERROR_DEVICE_LOST错误

**接下来直接说结论**  
能够为物体设置opaque flag的地方有4处：
1. BLAS的`VkAccelerationStructureGeometryKHR.flag = VK_GEOMETRY_OPAQUE_BIT_KHR`
2. TLAS的`VkAccelerationStructureInstanceKHR.flag = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR`
3. TLAS的`VkAccelerationStructureGeometryKHR.flag = VK_GEOMETRY_OPAQUE_BIT_KHR`
4. shader中`traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, origin, tmin, direction, tmax, 0);`

根据测试4的优先级最高，即可以覆盖其他所有。2可以覆盖1，但**3不能覆盖1** 。没设置opaque时一定要写anyhit shader！！！
