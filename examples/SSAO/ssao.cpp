#include "ssao.h"

VulkanExample::VulkanExample() : VulkanExampleBase()
{
    title = "SSAO";
    timerSpeed *= 0.5f;
    timer = 0.0f;

    camera.type = Camera::CameraType::firstperson;
    camera.position = { 1.0f, -0.75f, 0.0f };
    camera.setRotation(glm::vec3(0.0f, 90.0f, 0.0f));
    camera.setPerspective(60.0f, (float)width / (float)height, uniformBufferHost.zNear, uniformBufferHost.zFar);

    settings.validation = true;
    settings.fullscreen = false;
}

VulkanExample::~VulkanExample() {
    deferred.destroy(device);
    ssaoPass.destroy(device);
    blurPass.destroy(device);
    finalPass.destroy(device);
    uniformBuffer.destroy();
    paramBuffer.destroy();
    sampleBuffer.destroy();
    gaussianBuffer.destroy();
}

void VulkanExample::loadAssets() {
    const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices;// | vkglTF::FileLoadingFlags::PreMultiplyVertexColors;
    scene.loadFromFile(getAssetPath() + "models/sponza/sponza.gltf", vulkanDevice, queue, glTFLoadingFlags);
    //scene.loadFromFile(getAssetPath() + "models/mary.gltf", vulkanDevice, queue, glTFLoadingFlags);
}

void VulkanExample::getEnabledExtensions()
{
    enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
}

void VulkanExample::prepare() {
    VulkanExampleBase::prepare();
    loadAssets();
    prepareUniformBuffer();

    prepareDefer();
    prepareSSAO();
    prepareBlur();

    prepareDescriptor();
    preparePipeline();
    buildCommandBuffers();
    prepared = true;

    VkPhysicalDeviceProperties deviceProperties1;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties1);

    // 获取工作组的最大尺寸

    uint32_t maxWorkGroupSizeX = deviceProperties1.limits.maxComputeWorkGroupSize[0];
    uint32_t maxWorkGroupSizeY = deviceProperties1.limits.maxComputeWorkGroupSize[1];
    uint32_t maxWorkGroupSizeZ = deviceProperties1.limits.maxComputeWorkGroupSize[2];
    uint32_t maxWorkGroupInvocations = deviceProperties1.limits.maxComputeWorkGroupInvocations;

    printf("Max Work Group Size:\n");
    printf("  X: %u\n", maxWorkGroupSizeX);
    printf("  Y: %u\n", maxWorkGroupSizeY);
    printf("  Z: %u\n", maxWorkGroupSizeZ);
    printf("Max Work Group Invocations: %u\n", maxWorkGroupInvocations);
}

void VulkanExample::setupFrameBuffer()
{
    // Create frame buffers for every swap chain image
    frameBuffers.resize(swapChain.images.size());
    for (uint32_t i = 0; i < frameBuffers.size(); i++)
    {
        const VkImageView attachments[1] = {
            swapChain.imageViews[i],
        };
        VkFramebufferCreateInfo frameBufferCreateInfo{};
        frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferCreateInfo.renderPass = renderPass;
        frameBufferCreateInfo.attachmentCount = 1;
        frameBufferCreateInfo.pAttachments = attachments;
        frameBufferCreateInfo.width = width;
        frameBufferCreateInfo.height = height;
        frameBufferCreateInfo.layers = 1;
        VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
    }
}

void VulkanExample::setupRenderPass()
{
    std::array<VkAttachmentDescription, 1> attachments = {};
    // Color attachment
    attachments[0].format = swapChain.colorFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;
    subpassDescription.pDepthStencilAttachment = nullptr;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;
    subpassDescription.pResolveAttachments = nullptr;

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 1> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[0].dependencyFlags = 0;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}

void VulkanExample::createAttachment(VkFormat format, VkImageUsageFlags usage, Attachment& attachment) {
    attachment.format = format;

    VkImageCreateInfo imageInfo = vks::initializers::imageCreateInfo();
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vkCreateImage(device, &imageInfo, nullptr, &attachment.image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, attachment.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = vks::initializers::memoryAllocateInfo();
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &attachment.mem));
    VK_CHECK_RESULT(vkBindImageMemory(device, attachment.image, attachment.mem, 0));

    VkImageAspectFlags aspectMask = 0;
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    {
        aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (format >= VK_FORMAT_D16_UNORM_S8_UINT)
            aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = attachment.image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &attachment.imageView));

    auto samplerInfo = vks::initializers::samplerCreateInfo();
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &attachment.sampler));

    attachment.descriptor = vks::initializers::descriptorImageInfo(
        attachment.sampler,
        attachment.imageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

void VulkanExample::prepareDefer() {
    // attachments
    createAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, deferred.pos);
    createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, deferred.normal);
    createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, deferred.albedo);
    VkFormat attDepthFormat;
    VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &attDepthFormat);
    assert(validDepthFormat);
    createAttachment(attDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, deferred.depth);

    // renderpass
    std::array<VkAttachmentDescription, 4> attachments = {};
    {   // attachments
        // pos normal albedo
        for (int i = 0; i < 3; ++i) {
            attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
            attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        attachments[0].format = deferred.pos.format;
        attachments[1].format = deferred.normal.format;
        attachments[2].format = deferred.albedo.format;

        // depth
        attachments[3].format = attDepthFormat;
        attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    std::vector<VkAttachmentReference> refs(4);
    {   // attachment reference
        refs[0].attachment = 0;
        refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        refs[1].attachment = 1;
        refs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        refs[2].attachment = 2;
        refs[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        refs[3].attachment = 3;
        refs[3].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    std::array<VkSubpassDescription, 1> subpassDescription{};
    {   // subpass
        subpassDescription[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription[0].colorAttachmentCount = 3;
        subpassDescription[0].pColorAttachments = refs.data();
        subpassDescription[0].pDepthStencilAttachment = &refs[3];
    }
    
    std::array<VkSubpassDependency, 2> dependencies;
    {   // dependency
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
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
    }

    {   // renderpass
        VkRenderPassCreateInfo renderPassInfo = vks::initializers::renderPassCreateInfo();
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = static_cast<uint32_t>(subpassDescription.size());
        renderPassInfo.pSubpasses = subpassDescription.data();
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();
        VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &deferred.renderpass));
    }
    
    {   // framebuffer
        std::array<VkImageView, 4> attachments{
            deferred.pos.imageView, deferred.normal.imageView, deferred.albedo.imageView, 
            deferred.depth.imageView
        };
        VkFramebufferCreateInfo frameBufferCreateInfo = vks::initializers::framebufferCreateInfo();
        frameBufferCreateInfo.renderPass = deferred.renderpass;
        frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        frameBufferCreateInfo.pAttachments = attachments.data();
        frameBufferCreateInfo.width = deferred.width;
        frameBufferCreateInfo.height = deferred.height;
        frameBufferCreateInfo.layers = 1;
        VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &deferred.framebuffer));
    }
}

void VulkanExample::prepareSSAO() {
    createAttachment(VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, ssaoPass.ssao);
    // renderpass
    std::array<VkAttachmentDescription, 1> attachments = {};
    {   // attachments
        // ssao
        attachments[0].format = ssaoPass.ssao.format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    std::vector<VkAttachmentReference> refs(1);
    {   // attachment reference
        refs[0].attachment = 0;
        refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    std::array<VkSubpassDescription, 1> subpassDescription{};
    {   // subpass
        subpassDescription[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription[0].colorAttachmentCount = 1;
        subpassDescription[0].pColorAttachments = refs.data();
        subpassDescription[0].pDepthStencilAttachment = nullptr;
    }

    std::array<VkSubpassDependency, 1> dependencies;
    {   // dependency
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = 0;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        dependencies[0].dependencyFlags = 0;
    }

    {   // renderpass
        VkRenderPassCreateInfo renderPassInfo = vks::initializers::renderPassCreateInfo();
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = static_cast<uint32_t>(subpassDescription.size());
        renderPassInfo.pSubpasses = subpassDescription.data();
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();
        VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &ssaoPass.renderpass));
    }

    {   // framebuffer
        std::array<VkImageView, 1> attachments{
            ssaoPass.ssao.imageView
        };
        VkFramebufferCreateInfo frameBufferCreateInfo = vks::initializers::framebufferCreateInfo();
        frameBufferCreateInfo.renderPass = ssaoPass.renderpass;
        frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        frameBufferCreateInfo.pAttachments = attachments.data();
        frameBufferCreateInfo.width = ssaoPass.width;
        frameBufferCreateInfo.height = ssaoPass.height;
        frameBufferCreateInfo.layers = 1;
        VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &ssaoPass.framebuffer));
    }
}

void VulkanExample::prepareBlur() {
    createAttachment(VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, blurPass.blur);
    // renderpass
    std::array<VkAttachmentDescription, 1> attachments = {};
    {   // attachments
        // blur
        attachments[0].format = blurPass.blur.format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    std::vector<VkAttachmentReference> refs(1);
    {   // attachment reference
        refs[0].attachment = 0;
        refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    std::array<VkSubpassDescription, 1> subpassDescription{};
    {   // subpass
        subpassDescription[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription[0].colorAttachmentCount = 1;
        subpassDescription[0].pColorAttachments = refs.data();
        subpassDescription[0].pDepthStencilAttachment = nullptr;
    }

    std::array<VkSubpassDependency, 1> dependencies;
    {   // dependency
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = 0;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        dependencies[0].dependencyFlags = 0;
    }

    {   // renderpass
        VkRenderPassCreateInfo renderPassInfo = vks::initializers::renderPassCreateInfo();
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = static_cast<uint32_t>(subpassDescription.size());
        renderPassInfo.pSubpasses = subpassDescription.data();
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();
        VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &blurPass.renderpass));
    }

    {   // framebuffer
        std::array<VkImageView, 1> attachments{
            blurPass.blur.imageView
        };
        VkFramebufferCreateInfo frameBufferCreateInfo = vks::initializers::framebufferCreateInfo();
        frameBufferCreateInfo.renderPass = blurPass.renderpass;
        frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        frameBufferCreateInfo.pAttachments = attachments.data();
        frameBufferCreateInfo.width = blurPass.width;
        frameBufferCreateInfo.height = blurPass.height;
        frameBufferCreateInfo.layers = 1;
        VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &blurPass.framebuffer));
    }
}

void VulkanExample::buildCommandBuffers() {
    //std::cout << "build cmd\n";
    auto cmdBufInfo = vks::initializers::commandBufferBeginInfo();
    VkClearValue clearValues[10];
    VkViewport viewport;
    VkRect2D scissor;
    for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
    {
        VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
        {   /* deferred */
            clearValues[0].color = defaultClearColor;
            clearValues[1].color = defaultClearColor;
            clearValues[2].color = defaultClearColor;
            clearValues[3].depthStencil = { 1.0f, 0 };

            VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
            renderPassBeginInfo.renderPass = deferred.renderpass;
            renderPassBeginInfo.framebuffer = deferred.framebuffer;
            renderPassBeginInfo.renderArea.extent.width = deferred.width;
            renderPassBeginInfo.renderArea.extent.height = deferred.height;
            renderPassBeginInfo.clearValueCount = 4;
            renderPassBeginInfo.pClearValues = clearValues;

            vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            viewport = vks::initializers::viewport((float)deferred.width, -((float)deferred.height), 0.0f, 1.0f);
            viewport.y = (float)deferred.height;
            vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

            scissor = vks::initializers::rect2D(deferred.width, deferred.height, 0, 0);
            vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

            vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, deferred.pipelinelayout, 0, 1, &deferred.set, 0, nullptr);
            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, deferred.pipeline);
            scene.draw(drawCmdBuffers[i], vkglTF::RenderFlags::BindImages, deferred.pipelinelayout);

            vkCmdEndRenderPass(drawCmdBuffers[i]);
        }
        {   /* ssao */
            clearValues[0].color = defaultClearColor;

            VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
            renderPassBeginInfo.renderPass = ssaoPass.renderpass;
            renderPassBeginInfo.framebuffer = ssaoPass.framebuffer;
            renderPassBeginInfo.renderArea.extent.width = ssaoPass.width;
            renderPassBeginInfo.renderArea.extent.height = ssaoPass.height;
            renderPassBeginInfo.clearValueCount = 1;
            renderPassBeginInfo.pClearValues = clearValues;

            vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            viewport = vks::initializers::viewport((float)ssaoPass.width, -((float)ssaoPass.height), 0.0f, 1.0f);
            viewport.y = (float)ssaoPass.height;
            vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

            scissor = vks::initializers::rect2D(ssaoPass.width, ssaoPass.height, 0, 0);
            vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

            vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoPass.pipelinelayout, 0, 1, &ssaoPass.set, 0, nullptr);
            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoPass.pipeline);

            vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

            vkCmdEndRenderPass(drawCmdBuffers[i]);
        }
        {   /* blur */
            clearValues[0].color = defaultClearColor;

            VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
            renderPassBeginInfo.renderPass = blurPass.renderpass;
            renderPassBeginInfo.framebuffer = blurPass.framebuffer;
            renderPassBeginInfo.renderArea.extent.width = blurPass.width;
            renderPassBeginInfo.renderArea.extent.height = blurPass.height;
            renderPassBeginInfo.clearValueCount = 1;
            renderPassBeginInfo.pClearValues = clearValues;

            vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            viewport = vks::initializers::viewport((float)blurPass.width, -((float)blurPass.height), 0.0f, 1.0f);
            viewport.y = (float)blurPass.height;
            vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

            scissor = vks::initializers::rect2D(blurPass.width, blurPass.height, 0, 0);
            vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

            vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, blurPass.pipelinelayout, 0, 1, &blurPass.set, 0, nullptr);
            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, blurPass.pipeline);

            vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

            vkCmdEndRenderPass(drawCmdBuffers[i]);
        }
        {   /* final */
            clearValues[0].color = defaultClearColor;

            VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
            renderPassBeginInfo.renderPass = renderPass;
            renderPassBeginInfo.framebuffer = frameBuffers[i];
            renderPassBeginInfo.renderArea.extent.width = width;
            renderPassBeginInfo.renderArea.extent.height = height;
            renderPassBeginInfo.clearValueCount = 1;
            renderPassBeginInfo.pClearValues = clearValues;

            vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            viewport = vks::initializers::viewport((float)width, -((float)height), 0.0f, 1.0f);
            viewport.y = (float)height;
            vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

            scissor = vks::initializers::rect2D(width, height, 0, 0);
            vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

            vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, finalPass.pipelinelayout, 0, 1, &finalPass.set, 0, nullptr);
            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, finalPass.pipeline);
            vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

            drawUI(drawCmdBuffers[i]);

            vkCmdEndRenderPass(drawCmdBuffers[i]);
        }

        VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
    }
}

void VulkanExample::prepareUniformBuffer() {
    VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffer, sizeof(uniformBufferHost)));
    VK_CHECK_RESULT(uniformBuffer.map());

    VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &paramBuffer, sizeof(paramBufferHost)));
    VK_CHECK_RESULT(paramBuffer.map());

    {   /*prepare sampleBuffer*/
        VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &sampleBuffer, sizeof(sampleBufferHost)));
        VK_CHECK_RESULT(sampleBuffer.map());

        std::default_random_engine rndEngine(true ? 0 : (unsigned)time(nullptr));
        std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

        auto lerp = [](float a, float b, float f) -> float {
            return a + f * (b - a);
        };

        // Sample kernel
        for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; ++i)
        {
            glm::vec3 sample(rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine));
            sample = glm::normalize(sample);
            sample *= rndDist(rndEngine);
            float scale = float(i) / float(SSAO_KERNEL_SIZE);
            scale = lerp(0.1f, 1.0f, scale * scale);
            sampleBufferHost.kernel[i] = glm::vec4(sample * scale, 0.0f);
            // std::cout << "sample " << i << ": " << sampleBufferHost.kernel[i].x << "  " << sampleBufferHost.kernel[i].y << "  " << sampleBufferHost.kernel[i].z << std::endl;
        }

        // Random noise
        std::vector<glm::vec4> noiseValues(SSAO_NOISE_DIM * SSAO_NOISE_DIM);
        for (uint32_t i = 0; i < static_cast<uint32_t>(noiseValues.size()); i++) {
            noiseValues[i] = glm::vec4(rndDist(rndEngine) * 2.0f - 1.0f, rndDist(rndEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
        }
        // Upload as texture
        ssaoPass.noise.fromBuffer(
            noiseValues.data(), noiseValues.size() * sizeof(glm::vec4), 
            VK_FORMAT_R32G32B32A32_SFLOAT, SSAO_NOISE_DIM, SSAO_NOISE_DIM, vulkanDevice, queue, VK_FILTER_NEAREST);
        
        memcpy(sampleBuffer.mapped, &sampleBufferHost, sizeof(sampleBufferHost));
    }
    updateParamBuffer();

    VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &gaussianBuffer, sizeof(gaussianBufferHost)));
    VK_CHECK_RESULT(gaussianBuffer.map());
    updateGaussianBuffer();
}

void VulkanExample::calculateGaussianKernel(uint32_t kernelRadius, float sigma, glm::vec4* result) {
    float sum = 0.0f;
    for (int i = 0; i <= kernelRadius; ++i) {
        result[i].x = std::exp(-0.5f * std::pow(i / sigma, 2)) / (sigma * std::sqrt(2 * PI));
        sum += 2 * result[i].x;
    }
    sum -= result[0].x;
    float inv_sum = 1.0f / sum;
    for (int i = 0; i <= kernelRadius; ++i) {
        result[i].x *= inv_sum;
    }
}

void VulkanExample::updateGaussianBuffer() {
    calculateGaussianKernel(blurKernelRadius, sigma, gaussianBufferHost.coe);
    memcpy(gaussianBuffer.mapped, &gaussianBufferHost, sizeof(glm::vec4) * (blurKernelRadius + 1));
}


void VulkanExample::prepareDescriptor() {
    // pool
    std::vector<VkDescriptorPoolSize> poolSize = {
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 9),
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 9)
    };

    VkDescriptorPoolCreateInfo poolInfo = vks::initializers::descriptorPoolCreateInfo(poolSize, 4);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));

    std::vector<VkWriteDescriptorSet> writes(0);

    {   // deferred
        // layout
        std::vector<VkDescriptorSetLayoutBinding> layoutBinding = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1)
        };

        auto layoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(layoutBinding);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &deferred.layout));

        // set
        auto allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &deferred.layout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &deferred.set));

        // write
        writes.push_back(vks::initializers::writeDescriptorSet(deferred.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(deferred.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &paramBuffer.descriptor));
    }
    {   // ssao
        // layout
        std::vector<VkDescriptorSetLayoutBinding> layoutBinding = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 2),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
        };

        auto layoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(layoutBinding);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &ssaoPass.layout));

        // set
        auto allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &ssaoPass.layout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &ssaoPass.set));

        // write
        writes.push_back(vks::initializers::writeDescriptorSet(ssaoPass.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &sampleBuffer.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(ssaoPass.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &uniformBuffer.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(ssaoPass.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &paramBuffer.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(ssaoPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &deferred.pos.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(ssaoPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &deferred.normal.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(ssaoPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, &ssaoPass.noise.descriptor));
    }
    {   // blur
        // layout
        std::vector<VkDescriptorSetLayoutBinding> layoutBinding = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
        };

        auto layoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(layoutBinding);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &blurPass.layout));

        // set
        auto allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &blurPass.layout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &blurPass.set));

        // write
        writes.push_back(vks::initializers::writeDescriptorSet(blurPass.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &paramBuffer.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(blurPass.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &gaussianBuffer.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(blurPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &ssaoPass.ssao.descriptor));
    }
    {   // final
        // layout
        std::vector<VkDescriptorSetLayoutBinding> layoutBinding = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 6),
        };

        auto layoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(layoutBinding);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &finalPass.layout));

        // set
        auto allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &finalPass.layout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &finalPass.set));

        // write
        writes.push_back(vks::initializers::writeDescriptorSet(finalPass.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(finalPass.set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &paramBuffer.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(finalPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &deferred.pos.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(finalPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &deferred.normal.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(finalPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &deferred.albedo.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(finalPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, &ssaoPass.ssao.descriptor));
        writes.push_back(vks::initializers::writeDescriptorSet(finalPass.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6, &blurPass.blur.descriptor));
    }
    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}

void VulkanExample::updateUniformBuffer() {
    uniformBufferHost.projection = camera.matrices.perspective;
    uniformBufferHost.view = camera.matrices.view;
    uniformBufferHost.model = glm::mat4(1.0f);
    memcpy(uniformBuffer.mapped, &uniformBufferHost, sizeof(uniformBufferHost));
}

void VulkanExample::updateParamBuffer() {
    paramBufferHost.presentIdx = presentIdx;
    paramBufferHost.radius = radius;
    paramBufferHost.sampleNum = sampleNum;
    paramBufferHost.bias = bias;
    paramBufferHost.rangeCheck = rangeCheck;
    paramBufferHost.blurKernelRadius = blurKernelRadius;
    memcpy(paramBuffer.mapped, &paramBufferHost, sizeof(paramBufferHost));
}


void VulkanExample::preparePipeline() {
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

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(deferred.pipelinelayout, renderPass, 0);
    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pRasterizationState = &rasterizationStateCI;
    pipelineCI.pColorBlendState = &colorBlendStateCI;
    pipelineCI.pMultisampleState = &multisampleStateCI;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pDepthStencilState = &depthStencilStateCI;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    
    {   /* deferred */
        // layout
        std::vector<VkDescriptorSetLayout> setLayouts{ deferred.layout, vkglTF::descriptorSetLayoutImage };
        auto pipelinelayout = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());
        VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelinelayout, nullptr, &deferred.pipelinelayout));

        // pipeline
        pipelineCI.layout = deferred.pipelinelayout;
        pipelineCI.renderPass = deferred.renderpass;
        pipelineCI.subpass = 0;

        shaderStages[0] = loadShader(getShadersPath() + "SSAO/deferred.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "SSAO/deferred.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        pipelineCI.stageCount = 2;

        std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates = {
            vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE)
        };
        colorBlendStateCI.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
        colorBlendStateCI.pAttachments = blendAttachmentStates.data();

        auto vertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Tangent });
        pipelineCI.pVertexInputState = vertexInputState;
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &deferred.pipeline));
    }
    {   /* ssao */
        // layout
        std::vector<VkDescriptorSetLayout> setLayouts{ ssaoPass.layout };
        auto pipelinelayout = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());
        VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelinelayout, nullptr, &ssaoPass.pipelinelayout));

        // pipeline
        pipelineCI.layout = ssaoPass.pipelinelayout;
        pipelineCI.renderPass = ssaoPass.renderpass;
        pipelineCI.subpass = 0;

        shaderStages[0] = loadShader(getShadersPath() + "SSAO/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "SSAO/ssao.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        pipelineCI.stageCount = 2;

        colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

        auto emptyVertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
        pipelineCI.pVertexInputState = &emptyVertexInputState;
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &ssaoPass.pipeline));
    }
    {   /* blur */
        // layout
        std::vector<VkDescriptorSetLayout> setLayouts{ blurPass.layout };
        auto pipelinelayout = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());
        VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelinelayout, nullptr, &blurPass.pipelinelayout));

        // pipeline
        pipelineCI.layout = blurPass.pipelinelayout;
        pipelineCI.renderPass = blurPass.renderpass;
        pipelineCI.subpass = 0;

        shaderStages[0] = loadShader(getShadersPath() + "SSAO/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "SSAO/blur.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        pipelineCI.stageCount = 2;

        colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

        auto emptyVertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
        pipelineCI.pVertexInputState = &emptyVertexInputState;
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &blurPass.pipeline));
    }
    {   /* final*/
        // layout
        std::vector<VkDescriptorSetLayout> setLayouts{ finalPass.layout };
        auto pipelinelayout = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), setLayouts.size());
        VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelinelayout, nullptr, &finalPass.pipelinelayout));

        // pipeline
        pipelineCI.layout = finalPass.pipelinelayout;
        pipelineCI.renderPass = renderPass;
        pipelineCI.subpass = 0;

        shaderStages[0] = loadShader(getShadersPath() + "SSAO/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "SSAO/final.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        pipelineCI.stageCount = 2;

        colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

        auto emptyVertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
        pipelineCI.pVertexInputState = &emptyVertexInputState;
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &finalPass.pipeline));
    }
}

void VulkanExample::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
    if (overlay->header("Settings")) {
        overlay->text("Scene: Vulkan Scene");
        if (overlay->sliderFloat("radius", &radius, 0.0f, 5.0f)) {
            updateParamBuffer();
        }
        if (overlay->sliderInt("sampleNum", &sampleNum, 0, 64)) {
            updateParamBuffer();
        }
        if (overlay->sliderFloat("bias", &bias, -0.1f, 0.1f)) {
            updateParamBuffer();
        }
        if (overlay->checkBox("rangeCheck", &rangeCheck)) {
            updateParamBuffer();
        }
        if (overlay->sliderInt("blurKernelRadius", &blurKernelRadius, 0, 5)) {
            updateParamBuffer();
            updateGaussianBuffer();
        }
        if (overlay->sliderFloat("sigma", &sigma, 0.01f, 5.0f)) {
            updateGaussianBuffer();
        }
        if (overlay->comboBox("present", &presentIdx, presentName)) {
            updateParamBuffer();
        }
    }
}

void VulkanExample::getEnabledFeatures()
{
    if (deviceFeatures.samplerAnisotropy) {
        enabledFeatures.samplerAnisotropy = VK_TRUE;
    };
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