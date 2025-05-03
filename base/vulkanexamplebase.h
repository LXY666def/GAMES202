/*
* Vulkan Example base class
*
* Copyright (C) 2016-2024 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once
#pragma comment(linker, "/subsystem:windows")


#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <ShellScalingAPI.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <numeric>
#include <ctime>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <filesystem>
#include <sys/stat.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <numeric>
#include <array>

#include "vulkan/vulkan.h"

#include "keycodes.hpp"
#include "VulkanTools.h"
#include "VulkanDebug.h"
#include "VulkanUIOverlay.h"
#include "VulkanSwapChain.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"

#include "VulkanInitializers.hpp"
#include "camera.hpp"

class VulkanExampleBase
{
private:
	std::string getWindowTitle() const;
	uint32_t destWidth;
	uint32_t destHeight;
	bool resizing = false;
	void createPipelineCache();
	void createCommandPool();
	void createSynchronizationPrimitives();
	void createSurface();
	void createSwapChain();
	void createCommandBuffers();
	void destroyCommandBuffers();
protected:
	struct Settings {
		/** @brief Activates validation layers (and message output) when set to true */
		bool validation = false;
		/** @brief Set to true if fullscreen mode has been requested via command line */
		bool fullscreen = false;
		/** @brief Set to true if v-sync will be forced for the swapchain */
		bool vsync = false;
		/** @brief Enable UI overlay */
		bool overlay = true;
	} settings;
	

	// Frame counter to display fps
	uint32_t frameCounter = 0;
	uint32_t lastFPS = 0;
	// lastTimestamp: time when last `lastFPS` was updated. If (time_now -  lastTimestamp = 1 sec), then fps = `frameCounter`
	// tPrevEnd: time when last frame is finished. No actual usage now.
	std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp, tPrevEnd;
	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float timer = 0.0f;
	// Multiplier for speeding up (or slowing down) the global timer
	float timerSpeed = 0.25f;
	/** @brief Last frame time measured using a high performance timer (if available) */
	float frameTimer = 1.0f;

	// Vulkan objects
	VkInstance instance{ VK_NULL_HANDLE };
	std::vector<std::string> supportedInstanceExtensions;

	VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
	VkPhysicalDeviceProperties deviceProperties{};
	VkPhysicalDeviceFeatures deviceFeatures{};
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties{};

	/* virtual: features and extensions to be used */
	VkPhysicalDeviceFeatures enabledFeatures{};
	std::vector<const char*> enabledDeviceExtensions;
	std::vector<const char*> enabledInstanceExtensions;

	/** @brief Optional pNext structure for passing extension structures to device creation */
	void* deviceCreatepNextChain = nullptr;
	VkDevice device{ VK_NULL_HANDLE };
	// graphics queue
	VkQueue queue{ VK_NULL_HANDLE };
	
	VkCommandPool cmdPool{ VK_NULL_HANDLE };
	/** @brief Pipeline stages used to wait at for graphics queue submissions */
	VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// Contains command buffers and semaphores to be presented to the queue
	VkSubmitInfo submitInfo;
	// Command buffers used for rendering
	std::vector<VkCommandBuffer> drawCmdBuffers;

	// Depth buffer format (selected during Vulkan initialization)
	bool requiresStencil{ false };
	VkFormat depthFormat;
	VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };
	struct {
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
	} depthStencil{};	// this is only allowed to use in the final renderpass because it will be recreated when window is resized.
	VkRenderPass renderPass{ VK_NULL_HANDLE };
	std::vector<VkFramebuffer>frameBuffers;
	// Active frame buffer index
	uint32_t currentBuffer = 0;

	// Descriptor set pool
	VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
	// List of shader modules created (stored for cleanup)
	std::vector<VkShaderModule> shaderModules;

	VkPipelineCache pipelineCache{ VK_NULL_HANDLE };
	VulkanSwapChain swapChain;

	// Synchronization semaphores
	struct {
		// Swap chain image presentation
		VkSemaphore presentComplete;
		// Command buffer submission and execution
		VkSemaphore renderComplete;
	} semaphores;
	std::vector<VkFence> waitFences;
public:
	bool prepared = false;
	bool resized = false;
	bool viewUpdated = false;
	bool paused = false;
	uint32_t width = 1280;
	uint32_t height = 720;

	/** @brief Encapsulated physical and logical vulkan device */
	vks::VulkanDevice *vulkanDevice;
	
	Camera camera;

	std::string title = "Vulkan Example";
	std::string name = "vulkanExample";
	uint32_t apiVersion = VK_API_VERSION_1_3;

	/** @brief Default base class constructor */
	VulkanExampleBase();
	virtual ~VulkanExampleBase();
	/** @brief Setup the vulkan instance, enable required extensions and connect to the physical device (GPU) */
	bool initVulkan();
	/** @brief Prepares all Vulkan resources and functions required to run the sample */
	virtual void prepare();

	// [vulkan/fixed] create instance
	virtual VkResult createInstance();
	// [vulkan/virtual] customized function
	virtual void buildCommandBuffers();
	// [vulkan/fixed] customized function
	virtual void setupDepthStencil();
	// [vulkan/virtual] customized function
	virtual void setupFrameBuffer();
	// [vulkan/virtual] customized function
	virtual void setupRenderPass();
	// [vulkan/virtual] called before device is created
	virtual void getEnabledFeatures();
	// [vulkan/virtual] called before device is created
	virtual void getEnabledExtensions();

	/** @brief Loads a SPIR-V shader file for the given shader stage */
	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);
	// Returns the path to the root of the glsl shader directory.
	std::string getShadersPath() const;

	// OS specific
	HWND window;
	HINSTANCE windowInstance;
	// [window/fixed] set up window
	HWND setupWindow(HINSTANCE hinstance, WNDPROC wndproc);
	// [window/fixed] destroy and recreate resources
	void windowResize();
	// [window/virtual] called after windowResize()
	virtual void windowResized();
	void setupConsole(std::string title);
	void setupDPIAwareness();

	// [render/virtual]->nextFrame(): render loop
	virtual void renderLoop();
	// [render/fixed]->render()->updateOverlay(): render and count fps
	void nextFrame();
	// [render/virtual]
	virtual void render() = 0;
	// [render/fixed] acquire next image
	void prepareFrame();
	// [render/fixed] present image to the window
	void submitFrame();

	vks::UIOverlay ui;
	// [ui/fixed] draw ui
	void drawUI(const VkCommandBuffer commandBuffer);
	// [ui/fixed] update ui content
	void updateOverlay();
	// [ui/virtual] customize ui overlay
	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay);

	struct {
		struct {
			bool left = false;
			bool right = false;
			bool middle = false;
		} buttons;
		glm::vec2 position;
	} mouseState;
	// [interaction/fixed] handle users' keyboard and mouse input
	void handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	// [interaction/virtual] called after handleMessages()
	virtual void keyPressed(uint32_t);
	// [interaction/virtual] called after handleMessages()
	virtual void OnHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	// [interaction/fixed] handle mouse movement, called in handleMessages()
	void handleMouseMove(int32_t x, int32_t y);
	// [interaction/virtual] called after handleMouseMove()
	virtual void mouseMoved(double x, double y, bool& handled);

};

#include "Entrypoints.h"