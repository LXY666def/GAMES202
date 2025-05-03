#include "PCSS.h"

VulkanExample::VulkanExample() : VulkanExampleBase()
{
    title = "PCSS";
    timerSpeed *= 0.5f;
    timer = 0.0f;

    camera.type = Camera::CameraType::lookat;
    camera.setPosition(glm::vec3(0.0f, 0.0f, -12.5f));
    camera.setRotation(glm::vec3(-25.0f, -390.0f, 0.0f));
    camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);

    settings.validation = true;
    settings.fullscreen = false;
}

VulkanExample::~VulkanExample() {
    {
        vkDestroyRenderPass(device, renderObjects.renderpass, nullptr);
        vkDestroyFramebuffer(device, renderObjects.framebuffer, nullptr);
        vkDestroyImage(device, renderObjects.image, nullptr);
        vkDestroyImageView(device, renderObjects.imageview, nullptr);
        vkFreeMemory(device, renderObjects.mem, nullptr);
        vkDestroySampler(device, renderObjects.sampler, nullptr);
    }

    for (int i = 0; i < 2; ++i) {
        vkDestroyDescriptorSetLayout(device, renderpasses[i].descriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(device, renderpasses[i].pipelineLayout, nullptr);
        vkDestroyPipeline(device, renderpasses[i].pipeline, nullptr);
    }

    {
        vkDestroyPipeline(device, debug.pipeline, nullptr);
    }

    uniformBuffer.destroy();
    possionDisk.buffer.destroy();
}

void VulkanExample::loadAssets() {
    const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;// | vkglTF::FileLoadingFlags::PreMultiplyVertexColors;
    scene.loadFromFile(getAssetPath() + "models/mary.gltf", vulkanDevice, queue, glTFLoadingFlags);
    plane.loadFromFile(getAssetPath() + "models/plane.gltf", vulkanDevice, queue, glTFLoadingFlags);
}

void VulkanExample::prepare() {
    VulkanExampleBase::prepare();
    loadAssets();
    prepareDepthImageAndSampler();
    prepareDepthRenderpassAndFramebuffer();
    prepareUniformBuffer();
    updatePossionDisk();
    prepareDescriptor();
    preparePipeline();
    updatePushConstant();
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
            clearValues[0].depthStencil = { 1.0f, 0 };

            VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
            renderPassBeginInfo.renderPass = renderObjects.renderpass;
            renderPassBeginInfo.framebuffer = renderObjects.framebuffer;
            renderPassBeginInfo.renderArea.extent.width = renderObjects.imageSize;
            renderPassBeginInfo.renderArea.extent.height = renderObjects.imageSize;
            renderPassBeginInfo.clearValueCount = 1;
            renderPassBeginInfo.pClearValues = clearValues;

            vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            viewport = vks::initializers::viewport((float)renderObjects.imageSize, (float)renderObjects.imageSize, 0.0f, 1.0f);
            vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

            scissor = vks::initializers::rect2D(renderObjects.imageSize, renderObjects.imageSize, 0, 0);
            vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

            vkCmdSetDepthBias(drawCmdBuffers[i], depthBiasConstant, 0.0f, depthBiasSlope);

            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderpasses[0].pipeline);
            vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderpasses[0].pipelineLayout, 0, 1, &renderpasses[0].descriptorSet, 0, nullptr);
            scene.draw(drawCmdBuffers[i]);
            plane.draw(drawCmdBuffers[i]);

            vkCmdEndRenderPass(drawCmdBuffers[i]);
        }
        // renderpass1
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

            viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
            vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

            scissor = vks::initializers::rect2D(width, height, 0, 0);
            vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

            // Visualize shadow map
            if (ifdebug) {
                vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderpasses[1].pipelineLayout, 0, 1, &renderpasses[1].descriptorSet, 0, nullptr);
                vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, debug.pipeline);
                vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);
            }
            else {
                // Render the shadows scene
                vkCmdPushConstants(drawCmdBuffers[i], renderpasses[1].pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &coe);
                vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderpasses[1].pipelineLayout, 0, 1, &renderpasses[1].descriptorSet, 0, nullptr);
                vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderpasses[1].pipeline);
                scene.draw(drawCmdBuffers[i], vkglTF::RenderFlags::BindImages, renderpasses[1].pipelineLayout);
                plane.draw(drawCmdBuffers[i]);
            }

            drawUI(drawCmdBuffers[i]);

            vkCmdEndRenderPass(drawCmdBuffers[i]);
        }

        VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
    }
}

void VulkanExample::prepareDepthImageAndSampler() {
    VkImageCreateInfo imageInfo = vks::initializers::imageCreateInfo();
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = renderObjects.imageSize;
    imageInfo.extent.height = renderObjects.imageSize;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = renderObjects.depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VK_CHECK_RESULT(vkCreateImage(device, &imageInfo, nullptr, &renderObjects.image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, renderObjects.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = vks::initializers::memoryAllocateInfo();
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &renderObjects.mem));

    VK_CHECK_RESULT(vkBindImageMemory(device, renderObjects.image, renderObjects.mem, 0));

    auto viewInfo = vks::initializers::imageViewCreateInfo();
    viewInfo.image = renderObjects.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = renderObjects.depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &renderObjects.imageview));

    auto samplerInfo = vks::initializers::samplerCreateInfo();
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = samplerInfo.addressModeU;
    samplerInfo.addressModeW = samplerInfo.addressModeU;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &renderObjects.sampler));
}

void VulkanExample::prepareDepthRenderpassAndFramebuffer() {
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = renderObjects.depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderpassInfo = vks::initializers::renderPassCreateInfo();
    renderpassInfo.attachmentCount = 1;
    renderpassInfo.pAttachments = &depthAttachment;
    renderpassInfo.subpassCount = 1;
    renderpassInfo.pSubpasses = &subpass;
    renderpassInfo.dependencyCount = 2;
    renderpassInfo.pDependencies = dependencies.data();
    VK_CHECK_RESULT(vkCreateRenderPass(device, &renderpassInfo, nullptr, &renderObjects.renderpass));

    VkFramebufferCreateInfo framebufferInfo = vks::initializers::framebufferCreateInfo();
    framebufferInfo.renderPass = renderObjects.renderpass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &renderObjects.imageview;
    framebufferInfo.width = renderObjects.imageSize;
    framebufferInfo.height = renderObjects.imageSize;
    framebufferInfo.layers = 1;
    VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &renderObjects.framebuffer))
}

void VulkanExample::prepareUniformBuffer() {
    VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(uniformBufferHost)));
    VK_CHECK_RESULT(uniformBuffer.map());

    VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &possionDisk.buffer, sizeof(glm::vec4) * possionDisk.maxSampleNum));
    VK_CHECK_RESULT(possionDisk.buffer.map());
}

void VulkanExample::prepareDescriptor() {
    // pool
    std::vector<VkDescriptorPoolSize> poolSize = {
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3)
    };

    VkDescriptorPoolCreateInfo poolInfo = vks::initializers::descriptorPoolCreateInfo(poolSize, 2);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));

    // layout
    std::vector<VkDescriptorSetLayoutBinding> layoutBinding = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0)
    };
    
    VkDescriptorSetLayoutCreateInfo layoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(layoutBinding);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &renderpasses[0].descriptorSetLayout));

    layoutBinding.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1));
    layoutBinding.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2));
    layoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(layoutBinding);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &renderpasses[1].descriptorSetLayout));

    // set
    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &renderpasses[0].descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &renderpasses[0].descriptorSet));
    allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &renderpasses[1].descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &renderpasses[1].descriptorSet));

    // write
    std::vector<VkWriteDescriptorSet> descriptorWrite = {
        vks::initializers::writeDescriptorSet(renderpasses[0].descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor)
    };
    vkUpdateDescriptorSets(device, 1, descriptorWrite.data(), 0, nullptr);

    auto imageInfo = vks::initializers::descriptorImageInfo(renderObjects.sampler, renderObjects.imageview, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    descriptorWrite[0] = vks::initializers::writeDescriptorSet(renderpasses[1].descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor);
    descriptorWrite.push_back(vks::initializers::writeDescriptorSet(renderpasses[1].descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo));
    descriptorWrite.push_back(vks::initializers::writeDescriptorSet(renderpasses[1].descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &possionDisk.buffer.descriptor));
    vkUpdateDescriptorSets(device, descriptorWrite.size(), descriptorWrite.data(), 0, nullptr);

}

void VulkanExample::updateUniformBuffer() {
    updateLight();
    uniformBufferHost.projection = camera.matrices.perspective;
    uniformBufferHost.view = camera.matrices.view;
    uniformBufferHost.model = glm::mat4(1.0f);
    uniformBufferHost.lightPos = glm::vec4(lightPos, 1.0f);
    uniformBufferHost.depthMVP = depthMVP;
    uniformBufferHost.zNear = zNear;
    uniformBufferHost.zFar = zFar;
    memcpy(uniformBuffer.mapped, &uniformBufferHost, sizeof(uniformBufferHost));
}

void VulkanExample::updateLight() {
    lightPos.x = cos(glm::radians(timer * 360.0f)) * 50.0f;
    lightPos.y = -50.0f + sin(glm::radians(timer * 360.0f)) * 20.0f;
    lightPos.z = 25.0f + sin(glm::radians(timer * 360.0f)) * 5.0f;

    glm::mat4 depthProjectionMatrix = glm::perspective(lightFOV, 1.0f, zNear, zFar);
    glm::mat4 depthViewMatrix = glm::lookAt(lightPos/glm::vec3(2.0), glm::vec3(0.0f), glm::vec3(0, 1, 0));
    glm::mat4 depthModelMatrix = glm::mat4(1.0f);

    depthMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;
}

float rand_2to1() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    return dis(gen);
}

void VulkanExample::updatePossionDisk() {
    possionDisk.samples.clear();
    possionDisk.samples.resize(possionDisk.maxSampleNum);

    float ANGLE_STEP = PI2 * static_cast<float>(possionDisk.ringNum) / static_cast<float>(possionDisk.blockerSearchSampleNum);
    float INV_NUM_SAMPLES = 1.0f / static_cast<float>(possionDisk.blockerSearchSampleNum);

    float angle = rand_2to1() * PI2;
    float radius = INV_NUM_SAMPLES;
    float radiusStep = radius;
    for (int i = 0; i < possionDisk.blockerSearchSampleNum; i++) {
        possionDisk.samples[i].x = glm::cos(angle) * glm::pow(radius, 0.75f);
        possionDisk.samples[i].y = glm::sin(angle) * glm::pow(radius, 0.75f);
        radius += radiusStep;
        angle += ANGLE_STEP;
    }

    ANGLE_STEP = PI2 * static_cast<float>(possionDisk.ringNum) / static_cast<float>(possionDisk.PCFSampleNum);
    INV_NUM_SAMPLES = 1.0f / static_cast<float>(possionDisk.PCFSampleNum);

    angle = rand_2to1() * PI2;
    radius = INV_NUM_SAMPLES;
    radiusStep = radius;
    for (int i = 0; i < possionDisk.PCFSampleNum; i++) {
        possionDisk.samples[i].z = glm::cos(angle) * glm::pow(radius, 0.75f);
        possionDisk.samples[i].w = glm::sin(angle) * glm::pow(radius, 0.75f);
        radius += radiusStep;
        angle += ANGLE_STEP;
    }

    memcpy(possionDisk.buffer.mapped, possionDisk.samples.data(), possionDisk.samples.size() * sizeof(glm::vec4));
}

void VulkanExample::updatePushConstant() {
    coe.fov_radius_scale_pad = glm::vec4(lightFOV, lightRadius, scale, 0);
    coe.block_pcf_visualize_pad = glm::vec4(possionDisk.blockerSearchSampleNum, possionDisk.PCFSampleNum, visualizeIdx, 0);
}

void VulkanExample::preparePipeline() {
    auto pipelinelayout = vks::initializers::pipelineLayoutCreateInfo(&renderpasses[0].descriptorSetLayout);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelinelayout, nullptr, &renderpasses[0].pipelineLayout));

    std::array<VkDescriptorSetLayout, 2> setLayouts = { renderpasses[1].descriptorSetLayout, vkglTF::descriptorSetLayoutImage };
    pipelinelayout = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstant);
    pipelinelayout.pushConstantRangeCount = 1;
    pipelinelayout.pPushConstantRanges = &pushConstantRange;
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelinelayout, nullptr, &renderpasses[1].pipelineLayout));

    auto inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    auto rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    auto blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    auto colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    auto depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    auto viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
    auto multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    auto dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(renderpasses[1].pipelineLayout, renderPass, 0);
    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pRasterizationState = &rasterizationStateCI;
    pipelineCI.pColorBlendState = &colorBlendStateCI;
    pipelineCI.pMultisampleState = &multisampleStateCI;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pDepthStencilState = &depthStencilStateCI;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();

    shaderStages[0] = loadShader(getShadersPath() + "PCSS/debug.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader(getShadersPath() + "PCSS/debug.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
    pipelineCI.pVertexInputState = &emptyInputState;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &debug.pipeline));

    auto vertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal });
    pipelineCI.pVertexInputState = vertexInputState;
    rasterizationStateCI.cullMode = VK_CULL_MODE_BACK_BIT;
    shaderStages[0] = loadShader(getShadersPath() + "PCSS/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader(getShadersPath() + "PCSS/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &renderpasses[1].pipeline));

    rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
    rasterizationStateCI.depthBiasEnable = VK_TRUE;
    shaderStages[0] = loadShader(getShadersPath() + "PCSS/depth.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    pipelineCI.stageCount = 1;
    colorBlendStateCI.attachmentCount = 0;
    colorBlendStateCI.pAttachments = nullptr;
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    pipelineCI.layout = renderpasses[0].pipelineLayout;
    pipelineCI.renderPass = renderObjects.renderpass;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &renderpasses[0].pipeline));
}

void VulkanExample::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
    if (overlay->header("Settings")) {
        overlay->text("Scene: Vulkan Scene");
        if (overlay->checkBox("Show shadow map", &ifdebug)) {
            buildCommandBuffers();
        }
        overlay->checkBox("pause", &paused);
        if (overlay->sliderFloat("light radius", &lightRadius, 0, 1)) {
            updatePushConstant();
        }
        if (overlay->sliderFloat("depth bias constant", &depthBiasConstant, 0, 16)) {
            buildCommandBuffers();
        }
        if (overlay->sliderFloat("depth bias slope", &depthBiasSlope, 0, 16)) {
            buildCommandBuffers();
        }
        if (overlay->sliderInt("blocker samples", &possionDisk.blockerSearchSampleNum, 1, 64)) {
            updatePossionDisk();
            updatePushConstant();
        }
        if (overlay->sliderInt("PCF samples", &possionDisk.PCFSampleNum, 1, 64)) {
            updatePossionDisk();
            updatePushConstant();
        }
        if (overlay->comboBox("Visualize", &visualizeIdx, visualizeName)) {
            updatePushConstant();
        }
        if (overlay->sliderFloat("Visualize Scale", &scale, 0, 10)) {
            updatePushConstant();
        }
    }
}

void VulkanExample::render() {
    if (!prepared)
        return;
    if (!paused || camera.updated) {
        updateUniformBuffer();
    }

    VulkanExampleBase::prepareFrame();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VulkanExampleBase::submitFrame();
}

VULKAN_EXAMPLE_MAIN()