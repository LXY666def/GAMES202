#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

#define PI 3.141592653589793
#define PI2 6.283185307179586

#define SSAO_KERNEL_SIZE 64
#define SSAO_NOISE_DIM 64

class VulkanExample : public VulkanExampleBase
{
public:
    vkglTF::Model scene;
    std::vector<std::string> presentName{
        "deferred_pos",
        "deferred_normal",
        "deferred_albedo",
        "ssao",
        "deferred_final",
        "blur"
    };
    int presentIdx = 4;
    int sampleNum  = 64;
    float radius = 1.0f;
    float bias = 0.025f;
    int rangeCheck = 1;
    int blurKernelRadius = 2;
    float sigma = 1.0f;

    struct Attachment {
        VkImage image;
        VkImageView imageView;
        VkFormat format;
        VkSampler sampler;
        VkDeviceMemory mem;
        VkDescriptorImageInfo descriptor;
        void destory(VkDevice device) {
            vkDestroyImage(device, image, nullptr);
            vkDestroyImageView(device, imageView, nullptr);
            if (sampler) {
                vkDestroySampler(device, sampler, nullptr);
            }
            vkFreeMemory(device, mem, nullptr);
        }
    };

    struct {
        Attachment pos, normal, albedo, depth;
        VkRenderPass renderpass;
        VkFramebuffer framebuffer;

        VkPipelineLayout pipelinelayout;
        VkPipeline pipeline;

        VkDescriptorSetLayout layout;
        VkDescriptorSet set;

        uint32_t width = 1280;
        uint32_t height = 720;

        void destroy(VkDevice device) {
            pos.destory(device);
            normal.destory(device);
            albedo.destory(device);
            depth.destory(device);

            vkDestroyRenderPass(device, renderpass, nullptr);
            vkDestroyFramebuffer(device, framebuffer, nullptr);
            vkDestroyPipeline(device, pipeline, nullptr);
            vkDestroyPipelineLayout(device, pipelinelayout, nullptr);
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }
    } deferred;

    struct {
        vks::Texture2D noise;

        Attachment ssao;
        VkRenderPass renderpass;
        VkFramebuffer framebuffer;

        VkPipelineLayout pipelinelayout;
        VkPipeline pipeline;

        VkDescriptorSetLayout layout;
        VkDescriptorSet set;

        uint32_t width = 1280;
        uint32_t height = 720;

        void destroy(VkDevice device) {
            ssao.destory(device);
            noise.destroy();
            vkDestroyRenderPass(device, renderpass, nullptr);
            vkDestroyFramebuffer(device, framebuffer, nullptr);
            vkDestroyPipeline(device, pipeline, nullptr);
            vkDestroyPipelineLayout(device, pipelinelayout, nullptr);
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }
    } ssaoPass;

    struct {
        Attachment blur;
        VkRenderPass renderpass;
        VkFramebuffer framebuffer;

        VkPipelineLayout pipelinelayout;
        VkPipeline pipeline;

        VkDescriptorSetLayout layout;
        VkDescriptorSet set;

        uint32_t width = 1280;
        uint32_t height = 720;

        void destroy(VkDevice device) {
            blur.destory(device);
            vkDestroyRenderPass(device, renderpass, nullptr);
            vkDestroyFramebuffer(device, framebuffer, nullptr);
            vkDestroyPipeline(device, pipeline, nullptr);
            vkDestroyPipelineLayout(device, pipelinelayout, nullptr);
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }
    } blurPass;

    struct {
        VkPipelineLayout pipelinelayout;
        VkPipeline pipeline;

        VkDescriptorSetLayout layout;
        VkDescriptorSet set;

        void destroy(VkDevice device) {
            vkDestroyPipeline(device, pipeline, nullptr);
            vkDestroyPipelineLayout(device, pipelinelayout, nullptr);
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }
    } finalPass;

    // data
    vks::Buffer uniformBuffer;
    struct {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model;
        float zNear = 0.1f;
        float zFar = 256.0f;
    } uniformBufferHost;
    // param
    vks::Buffer paramBuffer;
    struct {
        int presentIdx;
        int sampleNum;
        float radius;
        float bias;
        int rangeCheck;
        int blurKernelRadius;
    } paramBufferHost;
    // sample
    vks::Buffer sampleBuffer;
    struct {
        glm::vec4 kernel[SSAO_KERNEL_SIZE];
    } sampleBufferHost;
    // gaussian
    vks::Buffer gaussianBuffer;
    struct {
        glm::vec4 coe[10];
    } gaussianBufferHost;

    VulkanExample();
    ~VulkanExample();

    void prepare() override;
    void buildCommandBuffers() override;
    void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;
    void render() override;
    void getEnabledFeatures() override;
    void getEnabledExtensions() override;
    void setupFrameBuffer() override;
    void setupRenderPass() override;

    void loadAssets();
    void prepareDefer();
    void prepareSSAO();
    void prepareBlur();
    void prepareUniformBuffer();
    void prepareDescriptor();
    void preparePipeline();
    void updateUniformBuffer();
    void updateParamBuffer();
    void updateGaussianBuffer();

    void createAttachment(VkFormat format, VkImageUsageFlags usage, Attachment& attachment);

    void calculateGaussianKernel(uint32_t kernelRadius, float sigma, glm::vec4* result);
};