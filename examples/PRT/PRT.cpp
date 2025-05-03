#include "PRT.h"

PRT::PRT() : VulkanExampleBase()
{
    title = "PRT";
    timerSpeed *= 0.5f;
    timer = 0.0f;

    camera.type = Camera::CameraType::lookat;
    camera.setPosition(glm::vec3(0.0f, 0.0f, -8.0f));
    camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
    camera.setPerspective(45.0f, (float)width / (float)height, 1.0f, 256.0f);

    settings.validation = true;
    settings.fullscreen = false;

    ui.subpass = 1;
}

PRT::~PRT() {
    envmapBuffer.destroy();
    // vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSet);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    uniformBuffer.destroy();

    vkDestroyPipeline(device, envmap.pipeline, nullptr);
    vkDestroyPipelineLayout(device, envmap.pipelineLayout, nullptr);
    // vkFreeDescriptorSets(device, descriptorPool, 1, &envmap.descriptorSet);
    vkDestroyDescriptorSetLayout(device, envmap.descriptorSetLayout, nullptr);
    vkDestroySampler(device, envmap.sampler, nullptr);
    vkDestroyImageView(device, envmap.imageview, nullptr);
    vkDestroyImage(device, envmap.image, nullptr);
    vkFreeMemory(device, envmap.mem, nullptr);
}

std::vector<glm::mat3> PRT::loadSHs(const std::string& filename) {
    std::vector<glm::mat3> matrices;
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "failed to open " << filename << std::endl;
        return matrices;
    }

    size_t numMatrices;
    file.read(reinterpret_cast<char*>(&numMatrices), sizeof(numMatrices));

    matrices.resize(numMatrices);
    for (size_t i = 0; i < numMatrices; ++i) {
        file.read(reinterpret_cast<char*>(&matrices[i][0][0]), sizeof(glm::mat3));
    }

    file.close();
    return matrices;
}
void PRT::loadEnvmap() {
    auto file_path = getAssetPath()+"..\\examples\\PRT\\precompute_light\\moon_noon.bin";

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "failed to open " << file_path << std::endl;
    }

    std::vector<float> data(27);
    file.read(reinterpret_cast<char*>(data.data()), sizeof(float) * 27);
    envmapSH.r[0] = glm::vec4(data[0], data[1], data[2], 0);
    envmapSH.r[1] = glm::vec4(data[3], data[4], data[5], 0);
    envmapSH.r[2] = glm::vec4(data[6], data[7], data[8], 0);
    envmapSH.g[0] = glm::vec4(data[9], data[10], data[11], 0);
    envmapSH.g[1] = glm::vec4(data[12], data[13], data[14], 0);
    envmapSH.g[2] = glm::vec4(data[15], data[16], data[17], 0);
    envmapSH.b[0] = glm::vec4(data[18], data[19], data[20], 0);
    envmapSH.b[1] = glm::vec4(data[21], data[22], data[23], 0);
    envmapSH.b[2] = glm::vec4(data[24], data[25], data[26], 0);
}

void PRT::prepareEnvmap() {
    int width, height, channels;
    auto file_path = getAssetPath() + "..\\examples\\PRT\\precompute_light\\moon_noon.png";
    unsigned char* imageData = stbi_load(file_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!imageData) {
        throw std::runtime_error("Failed to load image");
    }

    VkImageCreateInfo imageInfo = vks::initializers::imageCreateInfo();
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CHECK_RESULT(vkCreateImage(device, &imageInfo, nullptr, &envmap.image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, envmap.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = vks::initializers::memoryAllocateInfo();
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &envmap.mem));
    VK_CHECK_RESULT(vkBindImageMemory(device, envmap.image, envmap.mem, 0));

    vks::Buffer stagingImage{};
    VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingImage, width * height * 4, imageData));
    auto copyBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;
    VkImageMemoryBarrier imageMemoryBarrier = vks::initializers::imageMemoryBarrier();;
    imageMemoryBarrier.image = envmap.image;
    imageMemoryBarrier.subresourceRange = subresourceRange;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkCmdPipelineBarrier(
        copyBuffer,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    vkCmdCopyBufferToImage(copyBuffer, stagingImage.buffer, envmap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(
        copyBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);
    vulkanDevice->flushCommandBuffer(copyBuffer, queue, true);
    stagingImage.destroy();
    stbi_image_free(imageData);

    auto viewInfo = vks::initializers::imageViewCreateInfo();
    viewInfo.image = envmap.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imageInfo.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &envmap.imageview));

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
    VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &envmap.sampler));

    envmap.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    envmap.descriptor.imageView = envmap.imageview;
    envmap.descriptor.sampler = envmap.sampler;
}

void PRT::loadAssets() {
    const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices;
    cube.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
    std::vector<vkglTF::Vertex> vertices;
    scene.loadFromFile(getAssetPath() + "models/mary.gltf",
        vulkanDevice, queue, glTFLoadingFlags, 1.0f, &vertices, nullptr);

    std::vector<glm::mat3> shs = loadSHs(getAssetPath() + "models/mary.sh9.bin");
    loadEnvmap();

    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &vertexBuffer, sizeof(Vertex) * vertices.size()));

    std::vector<Vertex> vertexHost(vertices.size());
    for (int i = 0; i < vertices.size(); ++i) {
        vertexHost[i] = { vertices[i].pos, vertices[i].uv, shs[i] };
    }
    vks::Buffer stagingBuffer;
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer, sizeof(Vertex) * vertices.size(),
        vertexHost.data()));
    auto transferCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkBufferCopy copyRange = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = stagingBuffer.size
    };
    vkCmdCopyBuffer(transferCmd, stagingBuffer.buffer, vertexBuffer.buffer, 1, &copyRange);
    vulkanDevice->flushCommandBuffer(transferCmd, queue, true);

    stagingBuffer.destroy();
    vkDestroyBuffer(device, scene.vertices.buffer, nullptr);
    vkFreeMemory(device, scene.vertices.memory, nullptr);
    scene.vertices.buffer = vertexBuffer.buffer;
    scene.vertices.memory = vertexBuffer.memory;
}

void PRT::prepare() {
    VulkanExampleBase::prepare();
    loadAssets();
    prepareEnvmap();
    prepareUniformBuffer();
    prepareDescriptor();
    preparePipeline();
    buildCommandBuffers();
    prepared = true;
}

void PRT::prepareUniformBuffer() {
    VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(uniformBufferHost)));
    VK_CHECK_RESULT(uniformBuffer.map());

    VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &envmapBuffer, sizeof(envmapSH), &envmapSH));
    VK_CHECK_RESULT(envmapBuffer.map());

}

void PRT::prepareDescriptor() {
    // pool
    std::vector<VkDescriptorPoolSize> poolSize = {
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3),
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };

    VkDescriptorPoolCreateInfo poolInfo = vks::initializers::descriptorPoolCreateInfo(poolSize, 2);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));

    // layout
    std::vector<VkDescriptorSetLayoutBinding> layoutBinding = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1)
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(layoutBinding);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout));
    layoutBinding[1] = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1);
    layoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(layoutBinding);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &envmap.descriptorSetLayout));

    // set
    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
    allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &envmap.descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &envmap.descriptorSet));

    // write
    std::vector<VkWriteDescriptorSet> descriptorWrite = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &envmapBuffer.descriptor),

        vks::initializers::writeDescriptorSet(envmap.descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor),
        vks::initializers::writeDescriptorSet(envmap.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &envmap.descriptor)
    };
    vkUpdateDescriptorSets(device, 4, descriptorWrite.data(), 0, nullptr);
}

void PRT::preparePipeline() {
    std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptorSetLayout, vkglTF::descriptorSetLayoutImage };
    auto pipelinelayout = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), 2);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelinelayout, nullptr, &pipelineLayout));
    pipelinelayout = vks::initializers::pipelineLayoutCreateInfo(&envmap.descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelinelayout, nullptr, &envmap.pipelineLayout));

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

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pRasterizationState = &rasterizationStateCI;
    pipelineCI.pColorBlendState = &colorBlendStateCI;
    pipelineCI.pMultisampleState = &multisampleStateCI;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pDepthStencilState = &depthStencilStateCI;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.subpass = 1;

    shaderStages[0] = loadShader(getShadersPath() + "PRT/prt.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader(getShadersPath() + "PRT/prt.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipelineVertexInputStateCreateInfo inputState = vks::initializers::pipelineVertexInputStateCreateInfo();
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[5];
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, uv);
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, sh);
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, sh) + sizeof(glm::vec3);
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[4].offset = offsetof(Vertex, sh) + 2 * sizeof(glm::vec3);

    inputState.vertexBindingDescriptionCount = 1;
    inputState.pVertexBindingDescriptions = &bindingDescription;
    inputState.vertexAttributeDescriptionCount = 5;
    inputState.pVertexAttributeDescriptions = attributeDescriptions;
    pipelineCI.pVertexInputState = &inputState;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

    shaderStages[0] = loadShader(getShadersPath() + "PRT/envmap.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader(getShadersPath() + "PRT/envmap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    depthStencilStateCI.depthTestEnable = false;
    depthStencilStateCI.depthWriteEnable = false;
    rasterizationStateCI.cullMode = VK_CULL_MODE_FRONT_BIT;
    pipelineCI.subpass = 0;
    pipelineCI.layout = envmap.pipelineLayout;
    pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position});
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &envmap.pipeline));
}

void PRT::setupRenderPass() {
    std::array<VkAttachmentDescription, 2> attachments = {};
    // Color attachment
    attachments[0].format = swapChain.colorFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // Depth attachment
    attachments[1].format = depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::vector<VkSubpassDescription> subpassDescription(2);
    subpassDescription[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription[0].colorAttachmentCount = 1;
    subpassDescription[0].pColorAttachments = &colorReference;
    subpassDescription[0].pDepthStencilAttachment = nullptr;
    subpassDescription[0].inputAttachmentCount = 0;
    subpassDescription[0].pInputAttachments = nullptr;
    subpassDescription[0].preserveAttachmentCount = 0;
    subpassDescription[0].pPreserveAttachments = nullptr;
    subpassDescription[0].pResolveAttachments = nullptr;
    subpassDescription[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription[1].colorAttachmentCount = 1;
    subpassDescription[1].pColorAttachments = &colorReference;
    subpassDescription[1].pDepthStencilAttachment = &depthReference;
    subpassDescription[1].inputAttachmentCount = 0;
    subpassDescription[1].pInputAttachments = nullptr;
    subpassDescription[1].preserveAttachmentCount = 0;
    subpassDescription[1].pPreserveAttachments = nullptr;
    subpassDescription[1].pResolveAttachments = nullptr;

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 4> dependencies{};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 1;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[0].dependencyFlags = 0;

    dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].dstSubpass = 0;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask = 0;
    dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[1].dependencyFlags = 0;

    dependencies[2].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[2].dstSubpass = 1;
    dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[2].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[2].srcAccessMask = 0;
    dependencies[2].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[2].dependencyFlags = 0;

    dependencies[3].srcSubpass = 0;
    dependencies[3].dstSubpass = 1;
    dependencies[3].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[3].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[3].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[3].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[3].dependencyFlags = 0;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(subpassDescription.size());
    renderPassInfo.pSubpasses = subpassDescription.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}

void PRT::buildCommandBuffers() {
    auto cmdBufInfo = vks::initializers::commandBufferBeginInfo();
    VkClearValue clearValues[2];
    VkViewport viewport;
    VkRect2D scissor;
    for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
    {
        VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
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

        viewport = vks::initializers::viewport((float)width, -((float)height), 0.0f, 1.0f);
        viewport.y = (float)height;
        vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

        scissor = vks::initializers::rect2D(width, height, 0, 0);
        vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

        vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, envmap.pipelineLayout, 0, 1, &envmap.descriptorSet, 0, nullptr);
        vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, envmap.pipeline);
        cube.draw(drawCmdBuffers[i]);

        vkCmdNextSubpass(drawCmdBuffers[i], VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        scene.draw(drawCmdBuffers[i], vkglTF::RenderFlags::BindImages, pipelineLayout);

        drawUI(drawCmdBuffers[i]);

        vkCmdEndRenderPass(drawCmdBuffers[i]);
        VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
    }
}

void PRT::updateUniformBuffer() {
    auto flipY = glm::mat4(1.0f);
    flipY[1][1] = 1.f;
    uniformBufferHost.projection = flipY * camera.matrices.perspective;
    uniformBufferHost.view = camera.matrices.view;
    uniformBufferHost.model = glm::mat4(1.0f);
    uniformBufferHost.lightPos = glm::vec4(1.0f);
    memcpy(uniformBuffer.mapped, &uniformBufferHost, sizeof(uniformBufferHost));
}

void PRT::render() {
    if (!prepared)
        return;
    updateUniformBuffer();

    VulkanExampleBase::prepareFrame();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VulkanExampleBase::submitFrame();
}

void PRT::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
    if (overlay->header("Settings")) {
        overlay->text("PRT");
        if (overlay->checkBox("prt", &uniformBufferHost.settings[0])) {
            updateUniformBuffer();
        }
    }
}

void PRT::getEnabledFeatures()
{
    if (deviceFeatures.samplerAnisotropy) {
        enabledFeatures.samplerAnisotropy = VK_TRUE;
    };
}

void PRT::getEnabledExtensions()
{
    enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
}

PRT* vulkanExample;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (vulkanExample != NULL)
    {
        vulkanExample->handleMessages(hWnd, uMsg, wParam, lParam);
    }
    return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}
int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_  HINSTANCE hPrevInstance, _In_ LPSTR, _In_ int)
{
    vulkanExample = new PRT();
    vulkanExample->initVulkan();
    vulkanExample->setupWindow(hInstance, WndProc);
    vulkanExample->prepare();
    vulkanExample->renderLoop();
    delete(vulkanExample);
    return 0;
}