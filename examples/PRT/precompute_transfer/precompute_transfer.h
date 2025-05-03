#include "VulkanRaytracingSample.h"
#include "VulkanglTFModel.h"

class VulkanExample : public VulkanRaytracingSample
{
public:
	AccelerationStructure bottomLevelAS{};
	AccelerationStructure topLevelAS{};

	vks::Buffer transformBuffer;
	struct TransformedVertex {
		glm::vec3 pos;
		glm::vec3 normal;
	};

	std::vector<VkTransformMatrixKHR> transformMatrices{};
	std::vector<TransformedVertex> transformedVertices{};
	int sampleNum = 1000;
	vks::Buffer sampleBuffer;
	vks::Buffer shBuffer;
	std::vector<glm::mat3> shBufferHost{};

	struct GeometryNode {
		uint64_t vertexBufferDeviceAddress;
		uint64_t indexBufferDeviceAddress;
		int32_t textureIndexBaseColor;
		int32_t textureIndexOcclusion;
	};
	vks::Buffer geometryNodesBuffer;

	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};
	struct ShaderBindingTables {
		ShaderBindingTable raygen;
		ShaderBindingTable miss;
		ShaderBindingTable hit;
	} shaderBindingTables;

	struct UniformData {
		glm::vec3 vertex;
		float pad0;
		glm::vec3 normal;
		float sampleNum;
	} uniformData;
	vks::Buffer uniformBuffer;

	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

	vkglTF::Model model;

	VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeatures{};

	VulkanExample();
	~VulkanExample();

	void createAccelerationStructureBuffer(AccelerationStructure& accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);

	void createBottomLevelAccelerationStructure();

	void createTopLevelAccelerationStructure();

	void createShaderBindingTables();

	void createRayTracingPipeline();

	void createDescriptorSets();

	void createUniformBuffer();
	void createStorageBuffer();
	void generateUniformSamples(std::vector<glm::vec2>& buffer, int num);

	void handleResize();

	void buildCommandBuffers();

	void updateUniformBuffers(int idx);

	void getEnabledFeatures();
	void getEnabledExtensions();

	void loadAssets();

	void prepare();

	void render() override;
	void renderLoop() override;
	void write2file();
};
