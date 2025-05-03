# Base Code for Vulkan Example
`class VulkanExampleBase` provides a basic graphics pipeline framework for implementing rendering algorithm quickly and conveniently. The introductions are as follow:
### Inherit base class
Once you inherit the base class, you need to initialize certain variables to be used later.
```cpp
class VulkanExample : public VulkanExampleBase
{
    VulkanExample() : VulkanExampleBase()
    {
        title = "Shadow Mapping";
        timerSpeed *= 0.025f;
        timer = 0.2f;

        camera.type = Camera::CameraType::firstperson;
        camera.movementSpeed = 2.5f;
        camera.setPerspective(45.0f, (float)width / (float)height, zNear, zFar);
        camera.setPosition(glm::vec3(-0.12f, 1.14f, -2.25f));
        camera.setRotation(glm::vec3(-17.0f, 7.0f, 0.0f));

        ui.subpass = 2;

        enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        deviceCreatepNextChain = &enabledDynamicRenderingFeaturesKHR;

        settings.vsync = true;
        settings.fullscreen = true;
        settings.overlay = false;
        settings.validation = true;

        requiresStencil = true;
    }
}
```
* `title` is the name of the example which will be presented on the window.
* `timerSpeed` is to adjust the speed of timer.
* `timer` is to control the animation, clamped to [0, 1] and fps independent
* `camera` literally.
* `ui.subpass` determines in which subpass ui will be drawed if the default renderpass has several subpasses.
* `enabledInstanceExtensions` includes the instance extensions to be used.
* `deviceCreatepNextChain` is set for passing extension structures to device creation.
* `settings.vsync` determines whether to use vsync present mode.
* `settings.fullscreen` determines whether the window is fullscreen.
* `settings.overlay` determines whether to enable ui.
* `settings.validation` determines whether to enable validation layer. Note that it is also affected by macro `NDEBUG`.
* `requiresStencil` determines whether the default depth-stencil image requires stencil part.
### Override methods
* `virtual void prepare();`  
This method is used to prepare resources like descriptors, new renderpass etc.
```cpp
void prepare() override
{
    VulkanExampleBase::prepare();
    // ...
    prepared = true;
}
```
* `virtual void buildCommandBuffers();`  
This method defines the cmdbuffer. Remember to call `drawUI()` in the last subpass of the last renderpass.
```cpp
void buildCommandBuffers() override
{
    VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
    // ...
    drawUI(drawCmdBuffers[i]);
    vkCmdEndRenderPass(drawCmdBuffers[i]);
    VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
}
```
* `virtual void setupFrameBuffer();`  
This method creates customized framebuffer.
* `virtual void setupRenderPass();`  
This method creates customized renderpass.
* `virtual void getEnabledFeatures();`  
This method enables the customized device features.
```cpp
void getEnabledFeatures() override
{
    if (deviceFeatures.samplerAnisotropy) {
    	enabledFeatures.samplerAnisotropy = VK_TRUE;
    };
}
```
* `virtual void getEnabledExtensions();`  
This method enables the customized device extensions.
```cpp
void getEnabledExtensions() override
{
    enabledDeviceExtensions.append(/*...*/)
}
```
* `virtual void render() = 0;`  
This method updates resources and summits commands.
```cpp
void render() override
{
    if (!prepared)
    	return;
    updateUniformBuffers();
	
    VulkanExampleBase::prepareFrame();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VulkanExampleBase::submitFrame();
}
```
* `virtual void renderLoop();`
The default method is a rendering loop. You can override it for computation tasks.
* `virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay);`  
This method creates your own ui interaction logic.
```cpp
void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
{
    if (overlay->header("Settings")) {
    	if (overlay->sliderFloat("LOD bias", &uniformData.lodBias, 0.0f, (float)texture.mipLevels)) {
            updateUniformBuffers();
    	}
    }
}
```
### Callable methods
* `virtual void windowResized();`  
Called after `void windowResize();` which destroys and recreates objects after window has resized. For example, destroy and recreate additional customized objects associated with window.
* `virtual void keyPressed(uint32_t);`   
Called after `handleMessages()` and a key was pressed. It is used to define more customized logic after a specific key was pressed.
* `virtual void OnHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);`  
Called after(at the end of) `handleMessages()` as an extension. So technically we don't need `virtual void keyPressed(uint32_t);`
* `virtual void mouseMoved(double x, double y, bool& handled);`  
Called after `void handleMouseMove(int32_t x, int32_t y);` and before update camera. You can customize your own mouse move logic, and if you don't want to update camera later, just set param `handled` to `true`.
### Other
* The framework has defined the default render objects like renderpass, framebuffer, depthstencil. In this way, we bind ui rendering with them. So the default renderpass must be the last renderpass in a graphics pipeline, and pipeline object of ui is bind to the last subpass of the default renderpass. It means this last subpass has at least one color attachment -- swapchain.image, because ui is going to be presented so it must be directly drawed on swapchain.image. And since the shader code of ui drawing is fixed, so swapchain.image must be the first color attachment.
* for now we are not using frames-in-flight tech, so different frames share the same semaphores. And we use `vkQueueWaitIdle(queue)` for simplicity though `vkWaitForFence` is better.
* sometimes you need to change the asset/shader path in `VulkanTools.cpp`