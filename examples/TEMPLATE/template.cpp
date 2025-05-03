#include "template.h"

VulkanExample::VulkanExample() : VulkanExampleBase()
{
    title = "TEMPLATE";
    timerSpeed *= 0.5f;
    timer = 0.0f;

    camera.type = Camera::CameraType::lookat;
    camera.setPosition(glm::vec3(0.0f, 0.0f, -12.5f));
    camera.setRotation(glm::vec3(25.0f, -390.0f, 0.0f));
    camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);

    settings.validation = true;
    settings.fullscreen = false;
}

VulkanExample::~VulkanExample() {
    vkDestroyDescriptorSetLayout(device, renderpasses.descriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(device, renderpasses.pipelineLayout, nullptr);
    vkDestroyPipeline(device, renderpasses.pipeline, nullptr);

    uniformBuffer.destroy();
}

void VulkanExample::loadAssets() {
    const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices;// | vkglTF::FileLoadingFlags::PreMultiplyVertexColors;
    scene.loadFromFile(getAssetPath() + "models/mary.gltf", vulkanDevice, queue, glTFLoadingFlags);
    plane.loadFromFile(getAssetPath() + "models/plane.gltf", vulkanDevice, queue, glTFLoadingFlags);
}

void VulkanExample::prepare() {
    VulkanExampleBase::prepare();
    loadAssets();
    prepareUniformBuffer();
    prepareDescriptor();
    preparePipeline();
    buildCommandBuffers();
    prepared = true;
}

void VulkanExample::buildCommandBuffers() {
    auto cmdBufInfo = vks::initializers::commandBufferBeginInfo();
    VkClearValue clearValues[2];
    VkViewport viewport;
    VkRect2D scissor;
    for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
    {
        VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
        // renderpass0
        {
            clearValues[0].color = defaultClearColor;
            clearValues[1].depthStencil = { 1.0f, 0 };

            VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
            renderPassBeginInfo.renderPass = renderPass;
            renderPassBeginInfo.framebuffer = frameBuffers[i];
            renderPassBeginInfo.renderArea.extent.width = width;
            renderPassBeginInfo.renderArea.extent.height = height;
            renderPassBeginInfo.clearValueCount = 2;
            renderPassBeginInfo.pClearValues = clearValues;

            vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            viewport = vks::initializers::viewport((float)width, -(float)height, 0.0f, 1.0f);
            viewport.y = (float)height;
            vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

            scissor = vks::initializers::rect2D(width, height, 0, 0);
            vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

            // Render the shadows scene
            vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderpasses.pipelineLayout, 0, 1, &renderpasses.descriptorSet, 0, nullptr);
            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderpasses.pipeline);
            scene.draw(drawCmdBuffers[i], vkglTF::RenderFlags::BindImages, renderpasses.pipelineLayout);
            plane.draw(drawCmdBuffers[i]);

            drawUI(drawCmdBuffers[i]);

            vkCmdEndRenderPass(drawCmdBuffers[i]);
        }

        VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
    }
}

void VulkanExample::prepareUniformBuffer() {
    VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(uniformBufferHost)));
    VK_CHECK_RESULT(uniformBuffer.map());
}

void VulkanExample::prepareDescriptor() {
    // pool
    std::vector<VkDescriptorPoolSize> poolSize = {
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
    };

    VkDescriptorPoolCreateInfo poolInfo = vks::initializers::descriptorPoolCreateInfo(poolSize, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));

    // layout
    std::vector<VkDescriptorSetLayoutBinding> layoutBinding = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0)
    };

    auto layoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(layoutBinding);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &renderpasses.descriptorSetLayout));

    // set
    auto allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &renderpasses.descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &renderpasses.descriptorSet));

    // write
    auto descriptorWrite = vks::initializers::writeDescriptorSet(renderpasses.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor);
    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

}

void VulkanExample::updateUniformBuffer() {
    uniformBufferHost.projection = camera.matrices.perspective;
    uniformBufferHost.view = camera.matrices.view;
    uniformBufferHost.model = glm::mat4(1.0f);
    memcpy(uniformBuffer.mapped, &uniformBufferHost, sizeof(uniformBufferHost));
}


void VulkanExample::preparePipeline() {
    std::array<VkDescriptorSetLayout, 2> setLayouts = { renderpasses.descriptorSetLayout, vkglTF::descriptorSetLayoutImage };
    auto pipelinelayout = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());
    
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelinelayout, nullptr, &renderpasses.pipelineLayout));

    auto inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    auto rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    auto blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    auto colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    auto depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    auto viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
    auto multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    auto dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(renderpasses.pipelineLayout, renderPass, 0);
    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pRasterizationState = &rasterizationStateCI;
    pipelineCI.pColorBlendState = &colorBlendStateCI;
    pipelineCI.pMultisampleState = &multisampleStateCI;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pDepthStencilState = &depthStencilStateCI;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    shaderStages[0] = loadShader(getShadersPath() + "TEMPLATE/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader(getShadersPath() + "TEMPLATE/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    auto vertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal });
    pipelineCI.pVertexInputState = vertexInputState;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &renderpasses.pipeline));
}

void VulkanExample::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
    if (overlay->header("Settings")) {
        overlay->text("Scene: Vulkan Scene");
    }
}

void VulkanExample::getEnabledFeatures()
{
    if (deviceFeatures.samplerAnisotropy) {
        enabledFeatures.samplerAnisotropy = VK_TRUE;
    };
}

void VulkanExample::getEnabledExtensions()
{
    enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
}

void VulkanExample::render() {
    if (!prepared)
        return;
    updateUniformBuffer();

    VulkanExampleBase::prepareFrame();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VulkanExampleBase::submitFrame();
}

VULKAN_EXAMPLE_MAIN()