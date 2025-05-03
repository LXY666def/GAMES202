#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define PI 3.141592653589793
#define PI2 6.283185307179586

class VulkanExample : public VulkanExampleBase
{
public:
    bool ifdebug = false;

    float zNear = 1.0f;
    float zFar = 96.f;
    float lightFOV = glm::radians(45.0f);
    float lightRadius = 0.06f;  // visualization 0.03 for penumbra*1000
    float depthBiasConstant = 1.25f;
    float depthBiasSlope = 1.75f;
    float scale = 1.0f;
    vkglTF::Model scene;
    vkglTF::Model plane;

    std::vector<std::string> visualizeName{"diffuse", "shadow", "searchSize", "avgBlockDepth", "prenumbra"};
    int visualizeIdx = 0;

    struct PushConstant {
        glm::vec4 fov_radius_scale_pad;
        glm::ivec4 block_pcf_visualize_pad;
    } coe;

    struct {
        int ringNum = 10;
        int blockerSearchSampleNum = 64;
        int PCFSampleNum = 64;
        int maxSampleNum = 64;
        std::vector<glm::vec4> samples;
        vks::Buffer buffer;
    } possionDisk;

    // renderpass 0
    struct {
        VkRenderPass renderpass;
        VkFramebuffer framebuffer;

        VkImage image;
        VkFormat depthFormat{ VK_FORMAT_D16_UNORM };
        int imageSize{ 2048 };
        VkDeviceMemory mem;
        VkImageView imageview;
        VkDescriptorImageInfo descriptor;

        VkSampler sampler;
    } renderObjects;

    // binding
    struct {
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
    } renderpasses[2];

    struct {
        VkPipeline pipeline;
    } debug;

    // data
    vks::Buffer uniformBuffer;
    struct {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model;
        glm::mat4 depthMVP;
        glm::vec4 lightPos;
        float zNear;
        float zFar;
    } uniformBufferHost;

    glm::vec3 lightPos;
    glm::mat4 depthMVP;

    VulkanExample();
    ~VulkanExample();

    void prepare() override;
    void buildCommandBuffers() override;
    void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;
    void render() override;
    void loadAssets();

    void prepareDepthImageAndSampler();
    void prepareDepthRenderpassAndFramebuffer();
    void prepareUniformBuffer();
    void prepareDescriptor();
    void preparePipeline();

    void updateUniformBuffer();
    void updateLight();
    void updatePossionDisk();
    void updatePushConstant();

    void getEnabledFeatures() override
    {
        // Enable anisotropic filtering if supported
        if (deviceFeatures.samplerAnisotropy) {
            enabledFeatures.samplerAnisotropy = VK_TRUE;
        };
    }
};