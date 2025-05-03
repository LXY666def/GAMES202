#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define PI 3.141592653589793
#define PI2 6.283185307179586

class VulkanExample : public VulkanExampleBase
{
public:
    vkglTF::Model scene;
    vkglTF::Model plane;

    // binding
    struct {
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
    } renderpasses;

    // data
    vks::Buffer uniformBuffer;
    struct {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model;
    } uniformBufferHost;

    VulkanExample();
    ~VulkanExample();

    void prepare() override;
    void buildCommandBuffers() override;
    void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;
    void render() override;
    void getEnabledFeatures() override;
    void getEnabledExtensions() override;

    void loadAssets();
    void prepareUniformBuffer();
    void prepareDescriptor();
    void preparePipeline();
    void updateUniformBuffer();
};