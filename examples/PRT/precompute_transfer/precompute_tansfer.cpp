#include "precompute_transfer.h"


VulkanExample::VulkanExample() : VulkanRaytracingSample()
{
	title = "Ray tracing glTF model";
	camera.type = Camera::CameraType::lookat;
	camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 512.0f);
	camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
	camera.setTranslation(glm::vec3(0.0f, -0.1f, -1.0f));

	enableExtensions();

	// Buffer device address requires the 64-bit integer feature to be enabled
	enabledFeatures.shaderInt64 = VK_TRUE;

	enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

	settings.validation = true;
}

VulkanExample::~VulkanExample()
{
	if (device) {
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		deleteStorageImage();
		deleteAccelerationStructure(bottomLevelAS);
		deleteAccelerationStructure(topLevelAS);
		transformBuffer.destroy();
		shaderBindingTables.raygen.destroy();
		shaderBindingTables.miss.destroy();
		shaderBindingTables.hit.destroy();
		uniformBuffer.destroy();
		geometryNodesBuffer.destroy();
	}

	sampleBuffer.destroy();
	shBuffer.destroy();
}

void VulkanExample::createAccelerationStructureBuffer(AccelerationStructure& accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
{
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &accelerationStructure.buffer));
	VkMemoryRequirements memoryRequirements{};
	vkGetBufferMemoryRequirements(device, accelerationStructure.buffer, &memoryRequirements);
	VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
	memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
	VkMemoryAllocateInfo memoryAllocateInfo{};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &accelerationStructure.memory));
	VK_CHECK_RESULT(vkBindBufferMemory(device, accelerationStructure.buffer, accelerationStructure.memory, 0));
}

/*
	Create the bottom level acceleration structure that contains the scene's actual geometry (vertices, triangles)
*/
void VulkanExample::createBottomLevelAccelerationStructure()
{
	// Transform buffer
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&transformBuffer,
		static_cast<uint32_t>(transformMatrices.size()) * sizeof(VkTransformMatrixKHR),
		transformMatrices.data()));

	// Build
	// One geometry per glTF node, so we can index materials using gl_GeometryIndexEXT
	uint32_t maxPrimCount{ 0 };
	std::vector<uint32_t> maxPrimitiveCounts{};
	std::vector<VkAccelerationStructureGeometryKHR> geometries{};
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos{};
	std::vector<VkAccelerationStructureBuildRangeInfoKHR*> pBuildRangeInfos{};
	std::vector<GeometryNode> geometryNodes{};
	for (auto node : model.linearNodes) {
		if (node->mesh) {
			for (auto primitive : node->mesh->primitives) {
				if (primitive->indexCount > 0) {
					VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
					VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
					VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

					vertexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(model.vertices.buffer);// +primitive->firstVertex * sizeof(vkglTF::Vertex);
					indexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(model.indices.buffer) + primitive->firstIndex * sizeof(uint32_t);
					transformBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(transformBuffer.buffer) + static_cast<uint32_t>(geometryNodes.size()) * sizeof(VkTransformMatrixKHR);

					VkAccelerationStructureGeometryKHR geometry{};
					geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
					geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
					// geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
					geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
					geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
					geometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
					geometry.geometry.triangles.maxVertex = model.vertices.count;
					//geometry.geometry.triangles.maxVertex = primitive->vertexCount;
					geometry.geometry.triangles.vertexStride = sizeof(vkglTF::Vertex);
					geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
					geometry.geometry.triangles.indexData = indexBufferDeviceAddress;
					geometry.geometry.triangles.transformData = transformBufferDeviceAddress;
					geometries.push_back(geometry);
					maxPrimitiveCounts.push_back(primitive->indexCount / 3);
					maxPrimCount += primitive->indexCount / 3;

					VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
					buildRangeInfo.firstVertex = 0;
					buildRangeInfo.primitiveOffset = 0; // primitive->firstIndex * sizeof(uint32_t);
					buildRangeInfo.primitiveCount = primitive->indexCount / 3;
					buildRangeInfo.transformOffset = 0;
					buildRangeInfos.push_back(buildRangeInfo);

					GeometryNode geometryNode{};
					geometryNode.vertexBufferDeviceAddress = vertexBufferDeviceAddress.deviceAddress;
					geometryNode.indexBufferDeviceAddress = indexBufferDeviceAddress.deviceAddress;
					geometryNodes.push_back(geometryNode);
				}
			}
		}
	}
	for (auto& rangeInfo : buildRangeInfos) {
		pBuildRangeInfos.push_back(&rangeInfo);
	}

	vks::Buffer stagingBuffer;

	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		static_cast<uint32_t>(geometryNodes.size()) * sizeof(GeometryNode),
		geometryNodes.data()));

	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&geometryNodesBuffer,
		static_cast<uint32_t>(geometryNodes.size()) * sizeof(GeometryNode)));

	vulkanDevice->copyBuffer(&stagingBuffer, &geometryNodesBuffer, queue);

	stagingBuffer.destroy();

	// Get size info
	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
	accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = static_cast<uint32_t>(geometries.size());
	accelerationStructureBuildGeometryInfo.pGeometries = geometries.data();

	const uint32_t numTriangles = maxPrimitiveCounts[0];

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
	accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	// PFN_vkGetAccelerationStructureBuildSizesKHR)(
	//		VkDevice, VkAccelerationStructureBuildTypeKHR, 
	//		const VkAccelerationStructureBuildGeometryInfoKHR*, 
	//		const uint32_t*  pMaxPrimitiveCounts, 
	//		VkAccelerationStructureBuildSizesInfoKHR*);
	vkGetAccelerationStructureBuildSizesKHR(
		device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		maxPrimitiveCounts.data(),
		&accelerationStructureBuildSizesInfo);

	createAccelerationStructureBuffer(bottomLevelAS, accelerationStructureBuildSizesInfo);

	VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
	accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	accelerationStructureCreateInfo.buffer = bottomLevelAS.buffer;
	accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &bottomLevelAS.handle);

	// Create a small scratch buffer used during build of the bottom level acceleration structure
	ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

	accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationStructureBuildGeometryInfo.dstAccelerationStructure = bottomLevelAS.handle;
	accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

	const VkAccelerationStructureBuildRangeInfoKHR* buildOffsetInfo = buildRangeInfos.data();

	// Build the acceleration structure on the device via a one-time command buffer submission
	// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
	VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	vkCmdBuildAccelerationStructuresKHR(
		commandBuffer,
		1,
		&accelerationStructureBuildGeometryInfo,
		pBuildRangeInfos.data());
	vulkanDevice->flushCommandBuffer(commandBuffer, queue);

	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
	accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accelerationDeviceAddressInfo.accelerationStructure = bottomLevelAS.handle;
	bottomLevelAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

	deleteScratchBuffer(scratchBuffer);
}

/*
	The top level acceleration structure contains the scene's object instances
*/
void VulkanExample::createTopLevelAccelerationStructure()
{
	// We flip the matrix [1][1] = -1.0f to accomodate for the glTF up vector
	VkTransformMatrixKHR transformMatrix = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f };

	VkAccelerationStructureInstanceKHR instance{};
	instance.transform = transformMatrix;
	instance.instanceCustomIndex = 0;
	instance.mask = 0xFF;
	instance.instanceShaderBindingTableRecordOffset = 0;
	instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR | VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
	instance.accelerationStructureReference = bottomLevelAS.deviceAddress;

	// Buffer for instance data
	vks::Buffer instancesBuffer;
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&instancesBuffer,
		sizeof(VkAccelerationStructureInstanceKHR),
		&instance));

	VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
	instanceDataDeviceAddress.deviceAddress = getBufferDeviceAddress(instancesBuffer.buffer);

	VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
	accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	//accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
	accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

	// Get size info
	/*
	The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored. Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command, except that the hostAddress member of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
	*/
	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
	accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = 1;
	accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

	uint32_t primitive_count = 1;

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
	accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(
		device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		&primitive_count,
		&accelerationStructureBuildSizesInfo);

	createAccelerationStructureBuffer(topLevelAS, accelerationStructureBuildSizesInfo);

	VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
	accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	accelerationStructureCreateInfo.buffer = topLevelAS.buffer;
	accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &topLevelAS.handle);

	// Create a small scratch buffer used during build of the top level acceleration structure
	ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

	VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
	accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationBuildGeometryInfo.dstAccelerationStructure = topLevelAS.handle;
	accelerationBuildGeometryInfo.geometryCount = 1;
	accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
	accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
	accelerationStructureBuildRangeInfo.primitiveCount = 1;
	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
	accelerationStructureBuildRangeInfo.firstVertex = 0;
	accelerationStructureBuildRangeInfo.transformOffset = 0;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

	// Build the acceleration structure on the device via a one-time command buffer submission
	// Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
	VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	vkCmdBuildAccelerationStructuresKHR(
		commandBuffer,
		1,
		&accelerationBuildGeometryInfo,
		accelerationBuildStructureRangeInfos.data());
	vulkanDevice->flushCommandBuffer(commandBuffer, queue);

	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
	accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accelerationDeviceAddressInfo.accelerationStructure = topLevelAS.handle;

	deleteScratchBuffer(scratchBuffer);
	instancesBuffer.destroy();
}

/*
	Create the Shader Binding Tables that binds the programs and top-level acceleration structure

	SBT Layout used in this sample:

		/-----------\
		| raygen    |
		|-----------|
		| miss + shadow     |
		|-----------|
		| hit + any |
		\-----------/

*/
void VulkanExample::createShaderBindingTables() {
	const uint32_t handleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
	const uint32_t handleSizeAligned = vks::tools::alignedSize(rayTracingPipelineProperties.shaderGroupHandleSize, rayTracingPipelineProperties.shaderGroupHandleAlignment);
	const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
	const uint32_t sbtSize = groupCount * handleSizeAligned;

	std::vector<uint8_t> shaderHandleStorage(sbtSize);
	VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

	createShaderBindingTable(shaderBindingTables.raygen, 1);
	createShaderBindingTable(shaderBindingTables.miss, 1);

	// Copy handles
	memcpy(shaderBindingTables.raygen.mapped, shaderHandleStorage.data(), handleSize);
	// We are using two miss shaders, so we need to get two handles for the miss shader binding table
	memcpy(shaderBindingTables.miss.mapped, shaderHandleStorage.data() + handleSizeAligned, handleSize);
}

/*
	Create our ray tracing pipeline
*/
void VulkanExample::createRayTracingPipeline()
{
	const uint32_t imageCount = static_cast<uint32_t>(model.textures.size());

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: Top level acceleration structure
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0),
		// Binding 1: uniform samples
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1),
		// Binding 2: params
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 2),
		// Binding 3: sh results
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 3),
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

	VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

	/*
		Setup ray tracing shader groups
	*/
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	// Ray generation group
	{
		shaderStages.push_back(loadShader(getShadersPath() + "precompute_transfer/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
		VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
		shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
		shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(shaderGroup);
	}

	// Miss group
	{
		shaderStages.push_back(loadShader(getShadersPath() + "precompute_transfer/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
		VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
		shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
		shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(shaderGroup);
	}

	/*
		Create the ray tracing pipeline
	*/
	VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
	rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	rayTracingPipelineCI.pStages = shaderStages.data();
	rayTracingPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups.size());
	rayTracingPipelineCI.pGroups = shaderGroups.data();
	rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
	rayTracingPipelineCI.layout = pipelineLayout;
	VK_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &pipeline));
}

/*
	Create the descriptor sets used for the ray tracing dispatch
*/
void VulkanExample::createDescriptorSets()
{
	uint32_t imageCount = static_cast<uint32_t>(model.textures.size());
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 }
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

	VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = vks::initializers::writeDescriptorSetAccelerationStructureKHR();
	descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
	descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS.handle;

	VkWriteDescriptorSet accelerationStructureWrite{};
	accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	// The specialized acceleration structure descriptor has to be chained
	accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
	accelerationStructureWrite.dstSet = descriptorSet;
	accelerationStructureWrite.dstBinding = 0;
	accelerationStructureWrite.descriptorCount = 1;
	accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	VkDescriptorBufferInfo samplesDescriptor = {
		.buffer = sampleBuffer.buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE
	};
	VkDescriptorBufferInfo shDescriptor = {
		.buffer = shBuffer.buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE
	};

	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		// Binding 0: Top level acceleration structure
		accelerationStructureWrite,
		// Binding 1: Ray tracing result image
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &samplesDescriptor),
		// Binding 2: Uniform data
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &uniformBuffer.descriptor),
		// Binding 3: Geometry node information SSBO
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &shDescriptor),
	};

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

/*
	Create the uniform buffer used to pass matrices to the ray tracing ray generation shader
*/
void VulkanExample::createUniformBuffer()
{
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&uniformBuffer,
		sizeof(uniformData),
		&uniformData));
	VK_CHECK_RESULT(uniformBuffer.map());

}

void VulkanExample::generateUniformSamples(std::vector<glm::vec2>& buffer, int num) {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.0, 1.0);

	for (int i = 0; i < num; ++i) {
		float phi = 2 * M_PI * dis(gen);
		float z = dis(gen);
		float theta = glm::acos(2 * z - 1);

		buffer[i] = glm::vec2(theta, phi);
	}
}

void VulkanExample::createStorageBuffer() {
	{
		// create sample buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&sampleBuffer,
			sizeof(float) * sampleNum * 2));

		std::vector<glm::vec2> samplesHost(sampleNum);
		generateUniformSamples(samplesHost, sampleNum);
		vks::Buffer storageBuffer;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&storageBuffer,
			sizeof(float) * sampleNum * 2,
			(void*)samplesHost.data()));
		auto cmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy copyRange = {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = sizeof(float) * sampleNum * 2
		};
		vkCmdCopyBuffer(cmdBuffer, storageBuffer.buffer, sampleBuffer.buffer, 1, &copyRange);
		vulkanDevice->flushCommandBuffer(cmdBuffer, queue, true);
		storageBuffer.destroy();
		samplesHost.clear();
	}

	int vertexNum = model.vertices.count;
	{
		// create sh buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&shBuffer,
			sizeof(float) * 9));
		VK_CHECK_RESULT(shBuffer.map())
	}
}

/*
	If the window has been resized, we need to recreate the storage image and it's descriptor
*/
void VulkanExample::handleResize()
{
	// Update descriptor
	VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, storageImage.view, VK_IMAGE_LAYOUT_GENERAL };
	VkWriteDescriptorSet resultImageWrite = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor);
	vkUpdateDescriptorSets(device, 1, &resultImageWrite, 0, VK_NULL_HANDLE);
	resized = false;
}

/*
	Command buffer generation
*/
void VulkanExample::buildCommandBuffers()
{
	if (resized)
	{
		handleResize();
	}
	int samplePerDim = glm::pow((double)sampleNum, 1.0 / 3);

	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

	for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
	{
		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
		vkCmdFillBuffer(drawCmdBuffers[i], shBuffer.buffer, 0, shBuffer.size, 0);

		// We need a barrier to make sure all writes are finished before starting to write again
		VkMemoryBarrier memoryBarrier = vks::initializers::memoryBarrier();
		memoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
		/*
			Dispatch the ray tracing commands
		*/
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descriptorSet, 0, 0);

		VkStridedDeviceAddressRegionKHR emptySbtEntry = {};
		vkCmdTraceRaysKHR(
			drawCmdBuffers[i],
			&shaderBindingTables.raygen.stridedDeviceAddressRegion,
			&shaderBindingTables.miss.stridedDeviceAddressRegion,
			&emptySbtEntry,
			&emptySbtEntry,
			samplePerDim,
			samplePerDim,
			samplePerDim);
		
		VkBufferMemoryBarrier bufferMemoryBarrier = vks::initializers::bufferMemoryBarrier();
		bufferMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		bufferMemoryBarrier.buffer = shBuffer.buffer;
		bufferMemoryBarrier.offset = 0;
		bufferMemoryBarrier.size = shBuffer.size;

		vkCmdPipelineBarrier(drawCmdBuffers[i], 
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
			0, 
			0, nullptr,
			1, &bufferMemoryBarrier,
			0, nullptr);

		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void VulkanExample::updateUniformBuffers(int idx)
{
	uniformData.vertex = transformedVertices[idx].pos;
	uniformData.normal = transformedVertices[idx].normal;
	uniformData.sampleNum = sampleNum;
	memcpy(uniformBuffer.mapped, &uniformData, sizeof(uniformData));
}

void VulkanExample::getEnabledExtensions() {
	enabledDeviceExtensions.push_back(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);
}

void VulkanExample::getEnabledFeatures()
{
	static VkPhysicalDeviceShaderAtomicFloatFeaturesEXT floatFeatures = {};
	floatFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
	floatFeatures.shaderBufferFloat32AtomicAdd = VK_TRUE;

	// Enable features required for ray tracing using feature chaining via pNext		
	enabledBufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	enabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;
	enabledBufferDeviceAddresFeatures.pNext = &floatFeatures;

	enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
	enabledRayTracingPipelineFeatures.pNext = &enabledBufferDeviceAddresFeatures;

	enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
	enabledAccelerationStructureFeatures.pNext = &enabledRayTracingPipelineFeatures;

	physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
	physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
	physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
	physicalDeviceDescriptorIndexingFeatures.pNext = &enabledAccelerationStructureFeatures;

	deviceCreatepNextChain = &physicalDeviceDescriptorIndexingFeatures;

	enabledFeatures.samplerAnisotropy = VK_TRUE;
}

void VulkanExample::loadAssets()
{
	vkglTF::memoryPropertyFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	std::vector<vkglTF::Vertex> vertexHost;
	model.loadFromFile(getAssetPath() + "models/mary.gltf", vulkanDevice, queue, 0u, 1.0f, &vertexHost, nullptr);
	
	transformedVertices.clear();
	transformedVertices.resize(vertexHost.size());
	// Use transform matrices from the glTF nodes
	for (auto node : model.linearNodes) {
		if (node->mesh) {
			for (auto primitive : node->mesh->primitives) {
				if (primitive->indexCount > 0) {
					auto mat = node->getMatrix();
					VkTransformMatrixKHR transformMatrix{};
					auto m = glm::mat3x4(glm::transpose(mat));
					memcpy(&transformMatrix, (void*)&m, sizeof(glm::mat3x4));
					transformMatrices.push_back(transformMatrix);

					for (int i = primitive->firstVertex; i < primitive->firstVertex + primitive->vertexCount; ++i) {
						transformedVertices[i].pos = glm::vec3(mat * glm::vec4(vertexHost[i].pos, 1.0f));
						transformedVertices[i].normal = glm::vec3(mat * glm::vec4(vertexHost[i].normal, 0.0f));
					}
				}
			}
		}
	}
}

void VulkanExample::prepare()
{
	VulkanRaytracingSample::prepare();

	loadAssets();

	// Create the acceleration structures used to render the ray traced scene
	createBottomLevelAccelerationStructure();
	createTopLevelAccelerationStructure();

	// we dont need to present 
	// createStorageImage(swapChain.colorFormat, { width, height, 1 });
	createUniformBuffer();
	createStorageBuffer();
	createRayTracingPipeline();
	createShaderBindingTables();
	createDescriptorSets();
	buildCommandBuffers();
	prepared = true;
}

void VulkanExample::render()
{
	if (!prepared)
		return;

	submitInfo = vks::initializers::submitInfo();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &drawCmdBuffers[0];
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = nullptr;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK_RESULT(vkQueueWaitIdle(queue));
}

void VulkanExample::renderLoop() {
	int vertexNum = transformedVertices.size();
	float coe = 4 * M_PI / sampleNum;
	shBufferHost.resize(vertexNum);
	for (int i = 0; i < vertexNum; ++i) {
		updateUniformBuffers(i);
		render();
		memcpy(&shBufferHost[i], shBuffer.mapped, shBuffer.size);
		shBufferHost[i] = shBufferHost[i] * coe;
		if (i % 100 == 0) {
			std::cout << i << '/' << vertexNum << '\n';
		}
	}
	write2file();
	std::cout << "finish\n";
}

void VulkanExample::write2file() {
	auto filename = getAssetPath() + "models/mary.sh9.bin";
	std::ofstream file(filename, std::ios::binary);
	if (!file) {
		std::cerr << "unable to write to " << filename << std::endl;
		return;
	}

	size_t numMatrices = shBufferHost.size();
	file.write(reinterpret_cast<const char*>(&numMatrices), sizeof(numMatrices));

	for (const auto& mat : shBufferHost) {
		file.write(reinterpret_cast<const char*>(&mat[0][0]), sizeof(glm::mat3));
	}

	file.close();
	std::cout << "precomputed sh params write to " << filename << std::endl;
}

VULKAN_EXAMPLE_MAIN()
