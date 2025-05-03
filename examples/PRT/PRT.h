#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#include "stb_image.h"

class PRT : public VulkanExampleBase
{
public:
    struct Vertex {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::mat3 sh;
    };

    vkglTF::Model scene;
    vkglTF::Model cube;
    vks::Buffer vertexBuffer;

    VkDescriptorSetLayout descriptorSetLayout{};
    VkDescriptorSet descriptorSet{};
    vks::Buffer uniformBuffer;
    VkPipelineLayout pipelineLayout{};
    VkPipeline pipeline{};

    struct {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model;
        glm::vec4 lightPos;
        glm::ivec4 settings;
    } uniformBufferHost;

    struct {
        glm::mat4 r;
        glm::mat4 g;
        glm::mat4 b;
    } envmapSH;
    vks::Buffer envmapBuffer{};
    struct {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView imageview;
        VkDescriptorImageInfo descriptor;
        VkSampler sampler;

        VkDescriptorSetLayout descriptorSetLayout{};
        VkDescriptorSet descriptorSet{};
        VkPipelineLayout pipelineLayout{};
        VkPipeline pipeline{};
    } envmap;

    PRT();
    ~PRT();

    void loadAssets();
    void loadEnvmap();
    std::vector<glm::mat3> loadSHs(const std::string& filename);

    void prepare() override;
    void buildCommandBuffers() override;
    void render() override;
    void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;
    void getEnabledFeatures() override;
    void getEnabledExtensions() override;
    void setupRenderPass() override;

    void prepareUniformBuffer();
    void prepareDescriptor();
    void preparePipeline();
    void updateUniformBuffer();
    void prepareEnvmap();
};