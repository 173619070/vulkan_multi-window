#include "InstanceImpl.h"
#include <GLFW/glfw3.h>

static VkSampleCountFlags samples = VK_SAMPLE_COUNT_1_BIT;
static VkPhysicalDeviceType preferedGPU = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;


static VkLayerProperties* instLayerProps;
static uint32_t instance_layer_count;

void vkh_layers_check_init() {
	VK_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL));
	instLayerProps = (VkLayerProperties*)malloc(instance_layer_count * sizeof(VkLayerProperties));
	VK_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&instance_layer_count, instLayerProps));
}
void vkh_layers_check_release() {
	free(instLayerProps);
}

static VkExtensionProperties* instExtProps;
static uint32_t instExtCount;
void vkh_instance_extensions_check_init() {
	VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(NULL, &instExtCount, NULL));
	instExtProps = (VkExtensionProperties*)malloc(instExtCount * sizeof(VkExtensionProperties));
	VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(NULL, &instExtCount, instExtProps));
}
void vkh_instance_extensions_check_release() {
	free(instExtProps);
}



VkhPhyInfo vkh_phyinfo_create(VkPhysicalDevice phy,VkInstance inst) {
	VkhPhyInfo pi = (vkh_phy_t*)calloc(1, sizeof(vkh_phy_t));
	pi->phy = phy;

	vkGetPhysicalDeviceProperties(phy, &pi->properties);
	vkGetPhysicalDeviceMemoryProperties(phy, &pi->memProps);

	vkGetPhysicalDeviceQueueFamilyProperties(phy, &pi->queueCount, NULL);
	pi->queues = (VkQueueFamilyProperties*)malloc(pi->queueCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(phy, &pi->queueCount, pi->queues);

	//identify dedicated queues

	pi->cQueue = -1;
	pi->gQueue = -1;
	pi->tQueue = -1;
	pi->pQueue = -1;

	//try to find dedicated queues first
	for (uint32_t j = 0; j < pi->queueCount; j++) {
		bool present = false;
		switch (pi->queues[j].queueFlags) {
		case VK_QUEUE_GRAPHICS_BIT:
			if (glfwGetPhysicalDevicePresentationSupport(inst, phy, j)) {
				present = true;
			}
			if (present) {
				if (pi->pQueue < 0)
					pi->pQueue = j;
			}
			else if (pi->gQueue < 0)
				pi->gQueue = j;
			break;
		case VK_QUEUE_COMPUTE_BIT:
			if (pi->cQueue < 0)
				pi->cQueue = j;
			break;
		case VK_QUEUE_TRANSFER_BIT:
			if (pi->tQueue < 0)
				pi->tQueue = j;
			break;
		}
	}
	//try to find suitable queue if no dedicated one found
	for (uint32_t j = 0; j < pi->queueCount; j++) {
		if (pi->queues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			bool present = false;
			if (glfwGetPhysicalDevicePresentationSupport(inst, phy, j)) {
				present = true;
			}
			if (present) {
				if (pi->pQueue < 0)
					pi->pQueue = j;
			}
			else if (pi->gQueue < 0)
				pi->gQueue = j;
		}
		if ((pi->queues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (pi->gQueue < 0))
			pi->gQueue = j;
		if ((pi->queues[j].queueFlags & VK_QUEUE_COMPUTE_BIT) && (pi->cQueue < 0))
			pi->cQueue = j;
		if ((pi->queues[j].queueFlags & VK_QUEUE_TRANSFER_BIT) && (pi->tQueue < 0))
			pi->tQueue = j;
	}

	return pi;
}

void vkh_phyinfo_destroy(VkhPhyInfo phy) {
	free(phy->queues);
	free(phy);
}

InstanceImpl::~InstanceImpl()
{
	for (uint32_t i = 0; i < phyCount; i++)
		vkh_phyinfo_destroy(infos[i]);
	free(infos);
}


bool vkh_phyinfo_try_get_extension_properties(VkhPhyInfo phy, const char* name, const VkExtensionProperties* properties) {
	if (phy->pExtensionProperties == NULL) {
		VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(phy->phy, NULL, &phy->extensionCount, NULL));
		phy->pExtensionProperties = (VkExtensionProperties*)malloc(phy->extensionCount * sizeof(VkExtensionProperties));
		VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(phy->phy, NULL, &phy->extensionCount, phy->pExtensionProperties));
	}
	for (uint32_t i = 0; i < phy->extensionCount; i++) {
		if (strcmp(name, phy->pExtensionProperties[i].extensionName) == 0) {
			if (properties)
				properties = &phy->pExtensionProperties[i];
			return true;
		}
	}
	properties = NULL;
	return false;
}


bool vkh_phyinfo_create_presentable_queues(VkhPhyInfo phy, uint32_t queueCount, const float* queue_priorities, VkDeviceQueueCreateInfo* const qInfo) {
	qInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	if (phy->pQueue < 0)
		perror("No queue with presentable support found");
	else if (phy->queues[phy->pQueue].queueCount < queueCount)
		fprintf(stderr, "Request %d queues of family %d, but only %d available\n", queueCount, phy->pQueue, phy->queues[phy->pQueue].queueCount);
	else {
		qInfo->queueCount = queueCount;
		qInfo->queueFamilyIndex = phy->pQueue;
		qInfo->pQueuePriorities = queue_priorities;
		phy->queues[phy->pQueue].queueCount -= queueCount;
		return true;
	}
	return false;
}


bool vkengine_try_get_phyinfo(VkhPhyInfo* phys, uint32_t phyCount, VkPhysicalDeviceType gpuType, VkhPhyInfo* phy) {
	for (uint32_t i = 0; i < phyCount; i++) {
		if (phys[i]->properties.deviceType == gpuType) {
			*phy = phys[i];
			return true;
		}
	}
	return false;
}

VkhDevice vkh_device_import(VkInstance inst, VkPhysicalDevice phy, VkDevice vkDev) {
	VkhDevice dev = (vkh_device_t*)calloc(1, sizeof(vkh_device_t));
	dev->dev = vkDev;
	dev->phy = phy;
	dev->instance = inst;

	vkGetPhysicalDeviceMemoryProperties(phy, &dev->phyMemProps);
#ifdef VKH_USE_VMA
	VmaAllocatorCreateInfo allocatorInfo{}; 
	allocatorInfo.physicalDevice = phy;
	allocatorInfo.device = vkDev;
	vmaCreateAllocator(&allocatorInfo, &dev->allocator);
#else
#endif

	return dev;
}

InstanceImpl::InstanceImpl()
{
	if (glfwInit() == GLFW_FALSE) {
		perror("glfwInit failed");
		exit(-1);
	}

	if (glfwVulkanSupported() == GLFW_FALSE) {
		perror("glfwVulkanSupported return false.");
		exit(-1);
	}
	//创建实例
	const char* enabledLayers[10];
	const char* enabledExts[10];
	uint32_t enabledExtsCount = 0, enabledLayersCount = 0, phyCount = 0;

	vkh_layers_check_init();
	vkh_layers_check_release();

	uint32_t glfwReqExtsCount = 0;
	const char** gflwExts = glfwGetRequiredInstanceExtensions(&glfwReqExtsCount);
	
	for (uint32_t i = 0; i < glfwReqExtsCount; i++)
		enabledExts[i] = gflwExts[i];
	enabledExtsCount = glfwReqExtsCount;

	VkApplicationInfo app_infos = {};
	app_infos.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_infos.pApplicationName = "Test";
	app_infos.applicationVersion = VK_MAKE_VERSION(1, 0, 0);;
	app_infos.pEngineName = "TestEngine";
	app_infos.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_infos.apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0);
	
	VkInstanceCreateInfo inst_info = {};
	inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	inst_info.pApplicationInfo = &app_infos;
	inst_info.enabledExtensionCount = enabledExtsCount;
	inst_info.ppEnabledExtensionNames = enabledExts;
	inst_info.enabledLayerCount = enabledLayersCount;
	inst_info.ppEnabledLayerNames = enabledLayers;

	VK_CHECK_RESULT(vkCreateInstance(&inst_info, NULL, &inst));

	//枚举物理设备
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(inst, &phyCount, NULL));
	VkPhysicalDevice* phyDevices = (VkPhysicalDevice*)malloc(phyCount * sizeof(VkPhysicalDevice));
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(inst, &phyCount, phyDevices));
	//构建物理设备的present graphic compute transfer信息
	infos = (VkhPhyInfo*)malloc(phyCount * sizeof(VkhPhyInfo));
	for (uint32_t i = 0; i < phyCount; i++)
		infos[i] = vkh_phyinfo_create(phyDevices[i],inst);
	free(phyDevices);

	//从infos中选取一个支持preferedGPU特性的，如果不支持preferedGPU特性也不支持VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU，VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU特性，那么就使用infos[0]
	pi = 0;
	if (!vkengine_try_get_phyinfo(infos, phyCount, preferedGPU, &pi) &&
		!vkengine_try_get_phyinfo(infos, phyCount, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, &pi) &&
		!vkengine_try_get_phyinfo(infos, phyCount, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, &pi))
		pi = infos[0];

	//构建present的VkDeviceQueueCreateInfo，那么选择的VkhPhyInfo pi中，其pi->pQueue必须大于等于0有效
	uint32_t qCount = 0;
	float qPriorities[] = { 0.0 };
	VkDeviceQueueCreateInfo pQueueInfos[] = { {},{},{} };
	if (vkh_phyinfo_create_presentable_queues(pi, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;
	//if (vkh_phyinfo_create_compute_queues(pi, 1, qPriorities, &pQueueInfos[qCount]))
	//	qCount++;
	//if (vkh_phyinfo_create_transfer_queues(pi, 1, qPriorities, &pQueueInfos[qCount]))
	//	qCount++;

	enabledExtsCount = 0;
	if (vkh_phyinfo_try_get_extension_properties(pi, VK_KHR_SWAPCHAIN_EXTENSION_NAME, NULL))
		enabledExts[enabledExtsCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

	VkPhysicalDeviceFeatures features{};
	features.samplerAnisotropy = VK_TRUE;
	features.sampleRateShading = VK_TRUE;
	features.logicOp = VK_TRUE;
	//	features.shaderStorageImageWriteWithoutFormat	= VK_TRUE;
	//	features.fragmentStoresAndAtomics				= VK_TRUE;

	VkPhysicalDeviceVulkan12Features features_1_2{};
	features_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features_1_2.pNext = nullptr;

	VkDeviceCreateInfo device_info{};
	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.pNext = &features_1_2;
	device_info.flags = 0;
	device_info.queueCreateInfoCount = qCount;
	device_info.pQueueCreateInfos = (VkDeviceQueueCreateInfo*)&pQueueInfos;
	device_info.enabledLayerCount = 0;
	device_info.ppEnabledLayerNames = nullptr;
	device_info.enabledExtensionCount = enabledExtsCount;
	device_info.ppEnabledExtensionNames = enabledExts;
	device_info.pEnabledFeatures = &features;

	VkDevice dev;
	VK_CHECK_RESULT(vkCreateDevice(pi->phy, &device_info, NULL, &dev));
	vkhd = vkh_device_import(inst, pi->phy, dev);
}

#define VKVG_SURFACE_IMGS_REQUIREMENTS (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|\
	VK_FORMAT_FEATURE_TRANSFER_DST_BIT|VK_FORMAT_FEATURE_TRANSFER_SRC_BIT|VK_FORMAT_FEATURE_BLIT_SRC_BIT)

void _device_check_best_image_tiling(VkvgDevice dev, VkFormat format) {
	VkFormat stencilFormats[] = { VK_FORMAT_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };
	VkFormatProperties phyStencilProps = { 0 }, phyImgProps = { 0 };

	////check png blit format
	//VkFlags pngBlitFormats[] = { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM };
	//dev->pngStagFormat = VK_FORMAT_UNDEFINED;
	//for (int i = 0; i < 2; i++)
	//{
	//	vkGetPhysicalDeviceFormatProperties(dev->phy, pngBlitFormats[i], &phyImgProps);
	//	if ((phyImgProps.linearTilingFeatures & VKVG_PNG_WRITE_IMG_REQUIREMENTS) == VKVG_PNG_WRITE_IMG_REQUIREMENTS) {
	//		dev->pngStagFormat = pngBlitFormats[i];
	//		dev->pngStagTiling = VK_IMAGE_TILING_LINEAR;
	//		break;
	//	}
	//	else if ((phyImgProps.optimalTilingFeatures & VKVG_PNG_WRITE_IMG_REQUIREMENTS) == VKVG_PNG_WRITE_IMG_REQUIREMENTS) {
	//		dev->pngStagFormat = pngBlitFormats[i];
	//		dev->pngStagTiling = VK_IMAGE_TILING_OPTIMAL;
	//		break;
	//	}
	//}

	//if (dev->pngStagFormat == VK_FORMAT_UNDEFINED)
	//	LOG(VKVG_LOG_DEBUG, "vkvg create device failed: no suitable image format for png write\n");

	dev->stencilFormat = VK_FORMAT_UNDEFINED;
	dev->stencilAspectFlag = VK_IMAGE_ASPECT_STENCIL_BIT;
	//dev->supportedTiling = 0xff;

	vkGetPhysicalDeviceFormatProperties(dev->phy, format, &phyImgProps);

	if ((phyImgProps.optimalTilingFeatures & VKVG_SURFACE_IMGS_REQUIREMENTS) == VKVG_SURFACE_IMGS_REQUIREMENTS) {
		for (int i = 0; i < 4; i++)
		{
			vkGetPhysicalDeviceFormatProperties(dev->phy, stencilFormats[i], &phyStencilProps);
			if (phyStencilProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				dev->stencilFormat = stencilFormats[i];
				if (i > 0)
					dev->stencilAspectFlag |= VK_IMAGE_ASPECT_DEPTH_BIT;
				dev->supportedTiling = VK_IMAGE_TILING_OPTIMAL;
				return;
			}
		}
	}
	if ((phyImgProps.linearTilingFeatures & VKVG_SURFACE_IMGS_REQUIREMENTS) == VKVG_SURFACE_IMGS_REQUIREMENTS) {
		for (int i = 0; i < 4; i++)
		{
			vkGetPhysicalDeviceFormatProperties(dev->phy, stencilFormats[i], &phyStencilProps);
			if (phyStencilProps.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				dev->stencilFormat = stencilFormats[i];
				if (i > 0)
					dev->stencilAspectFlag |= VK_IMAGE_ASPECT_DEPTH_BIT;
				dev->supportedTiling = VK_IMAGE_TILING_LINEAR;
				return;
			}
		}
	}
	//dev->status = VKVG_STATUS_INVALID_FORMAT;
	//LOG(VKVG_LOG_ERR, "vkvg create device failed: image format not supported: %d\n", format);
}

VkhQueue _init_queue(VkhDevice dev) {
	VkhQueue q = (vkh_queue_t*)calloc(1, sizeof(vkh_queue_t));
	q->dev = dev;
	return q;
}
VkhQueue vkh_queue_create(VkhDevice dev, uint32_t familyIndex, uint32_t qIndex) {
	VkhQueue q = _init_queue(dev);
	q->familyIndex = familyIndex;
	vkGetDeviceQueue(dev->dev, familyIndex, qIndex, &q->queue);
	return q;
}
VkCommandPool vkh_cmd_pool_create(VkhDevice dev, uint32_t qFamIndex, VkCommandPoolCreateFlags flags) {
	VkCommandPool cmdPool;
	VkCommandPoolCreateInfo cmd_pool_info = {};
	cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmd_pool_info.pNext = NULL;
	cmd_pool_info.queueFamilyIndex = qFamIndex;
	cmd_pool_info.flags = flags;
	VK_CHECK_RESULT(vkCreateCommandPool(dev->dev, &cmd_pool_info, NULL, &cmdPool));
	return cmdPool;
}
VkCommandBuffer vkh_cmd_buff_create(VkhDevice dev, VkCommandPool cmdPool, VkCommandBufferLevel level) {
	VkCommandBuffer cmdBuff;
	VkCommandBufferAllocateInfo cmd = {}; 
	cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmd.pNext = NULL;
	cmd.commandPool = cmdPool;
	cmd.level = level;
	cmd.commandBufferCount = 1;
	VK_CHECK_RESULT(vkAllocateCommandBuffers(dev->dev, &cmd, &cmdBuff));
	return cmdBuff;
}
VkFence vkh_fence_create(VkhDevice dev) {
	VkFence fence;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = NULL;
	fenceInfo.flags = 0;
	VK_CHECK_RESULT(vkCreateFence(dev->dev, &fenceInfo, NULL, &fence));
	return fence;
}
VkFence vkh_fence_create_signaled(VkhDevice dev) {
	VkFence fence;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = NULL;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VK_CHECK_RESULT(vkCreateFence(dev->dev, &fenceInfo, NULL, &fence));
	return fence;
}
VkSemaphore vkh_semaphore_create(VkhDevice dev) {
	VkSemaphore semaphore;
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = NULL;
	info.flags = 0;
	VK_CHECK_RESULT(vkCreateSemaphore(dev->dev, &info, NULL, &semaphore));
	return semaphore;
}
void _device_create_pipeline_cache(VkvgDevice dev) {

	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = { pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
	VK_CHECK_RESULT(vkCreatePipelineCache(dev->vkDev, &pipelineCacheCreateInfo, NULL, &dev->pipelineCache));
}

VkRenderPass _device_createRenderPassNoResolve(VkvgDevice dev, VkAttachmentLoadOp loadOp, VkAttachmentLoadOp stencilLoadOp)
{
	VkAttachmentDescription attColor = {};
	attColor.format = VK_FORMAT_B8G8R8A8_UNORM;
	attColor.samples = dev->samples;
	attColor.loadOp = loadOp;
	attColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attColor.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attColor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attColor.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attColor.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription attDS = {};
	attDS.format = dev->stencilFormat;
	attDS.samples = dev->samples;
	attDS.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attDS.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attDS.stencilLoadOp = stencilLoadOp;
	attDS.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	attDS.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attDS.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


	VkAttachmentDescription attachments[] = { attColor,attDS };
	VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference dsRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorRef;
	subpassDescription.pDepthStencilAttachment = &dsRef;

	VkSubpassDependency dependencies[] =
	{
		{ VK_SUBPASS_EXTERNAL, 0,
		  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		  VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		  VK_DEPENDENCY_BY_REGION_BIT},
		{ 0, VK_SUBPASS_EXTERNAL,
		  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
		  VK_DEPENDENCY_BY_REGION_BIT},
	};

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies;

	VkRenderPass rp;
	VK_CHECK_RESULT(vkCreateRenderPass(dev->vkDev, &renderPassInfo, NULL, &rp));
	return rp;
}

void _device_createDescriptorSetLayout(VkvgDevice dev) {

	VkDescriptorSetLayoutBinding dsLayoutBinding =
	{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
	VkDescriptorSetLayoutCreateInfo dsLayoutCreateInfo = {};
	dsLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dsLayoutCreateInfo.bindingCount = 1;
	dsLayoutCreateInfo.pBindings = &dsLayoutBinding;
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(dev->vkDev, &dsLayoutCreateInfo, NULL, &dev->dslFont));
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(dev->vkDev, &dsLayoutCreateInfo, NULL, &dev->dslSrc));
	dsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(dev->vkDev, &dsLayoutCreateInfo, NULL, &dev->dslGrad));

	VkPushConstantRange pushConstantRange[] = {
		{VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(push_constants)},
		//{VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push_constants)}
	};
	VkDescriptorSetLayout dsls[] = { dev->dslFont,dev->dslSrc,dev->dslGrad };

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { };
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = (VkPushConstantRange*)&pushConstantRange;
	pipelineLayoutCreateInfo.setLayoutCount = 3;
	pipelineLayoutCreateInfo.pSetLayouts = dsls;
	VK_CHECK_RESULT(vkCreatePipelineLayout(dev->vkDev, &pipelineLayoutCreateInfo, NULL, &dev->pipelineLayout));
}


void _device_setupPipelines(VkvgDevice dev)
{
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = { };
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.renderPass = dev->renderPass;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = { };
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

	VkPipelineRasterizationStateCreateInfo rasterizationState = { };
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.cullMode = VK_CULL_MODE_NONE;
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.depthClampEnable = VK_FALSE;
	rasterizationState.rasterizerDiscardEnable = VK_FALSE;
	rasterizationState.depthBiasEnable = VK_FALSE;
	rasterizationState.lineWidth = 1.0f;

	VkPipelineColorBlendAttachmentState blendAttachmentState = {};
	blendAttachmentState.colorWriteMask = 0x0;
	blendAttachmentState.blendEnable = VK_TRUE;
#ifdef VKVG_PREMULT_ALPHA
	blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
#else
	blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
#endif


	VkPipelineColorBlendStateCreateInfo colorBlendState = {};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &blendAttachmentState;

	/*failOp,passOp,depthFailOp,compareOp, compareMask, writeMask, reference;*/
	VkStencilOpState polyFillOpState = { VK_STENCIL_OP_KEEP,VK_STENCIL_OP_INVERT,	VK_STENCIL_OP_KEEP,VK_COMPARE_OP_EQUAL,STENCIL_CLIP_BIT,STENCIL_FILL_BIT,0 };
	VkStencilOpState clipingOpState = { VK_STENCIL_OP_ZERO,VK_STENCIL_OP_REPLACE,VK_STENCIL_OP_KEEP,VK_COMPARE_OP_EQUAL,STENCIL_FILL_BIT,STENCIL_ALL_BIT, 0x2 };
	VkStencilOpState stencilOpState = { VK_STENCIL_OP_KEEP,VK_STENCIL_OP_ZERO,	VK_STENCIL_OP_KEEP,VK_COMPARE_OP_EQUAL,STENCIL_FILL_BIT,STENCIL_FILL_BIT,0x1 };

	VkPipelineDepthStencilStateCreateInfo dsStateCreateInfo = { };
	dsStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	dsStateCreateInfo.depthTestEnable = VK_FALSE;
	dsStateCreateInfo.depthWriteEnable = VK_FALSE;
	dsStateCreateInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;
	dsStateCreateInfo.stencilTestEnable = VK_TRUE;
	dsStateCreateInfo.front = polyFillOpState;
	dsStateCreateInfo.back = polyFillOpState;

	VkDynamicState dynamicStateEnables[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
		VK_DYNAMIC_STATE_STENCIL_REFERENCE,
		VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
	};
	VkPipelineDynamicStateCreateInfo dynamicState = { };
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStateEnables;

	VkPipelineViewportStateCreateInfo viewportState = { };
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1; viewportState.scissorCount = 1;


		VkPipelineMultisampleStateCreateInfo multisampleState = {};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleState.rasterizationSamples = dev->samples;
	/*if (dev->samples != VK_SAMPLE_COUNT_1_BIT){
		multisampleState.sampleShadingEnable = VK_TRUE;
		multisampleState.minSampleShading = 0.5f;
	}*/
		VkVertexInputBindingDescription vertexInputBinding = {};
		vertexInputBinding.binding = 0;
		vertexInputBinding.stride = sizeof(Vertex);
		vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertexInputAttributs[3] = {
		{0, 0, VK_FORMAT_R32G32_SFLOAT,		0},
		{1, 0, VK_FORMAT_R8G8B8A8_UNORM,	8},
		{2, 0, VK_FORMAT_R32G32B32_SFLOAT, 12}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState = {};
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputState.vertexBindingDescriptionCount = 1;
	vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
	vertexInputState.vertexAttributeDescriptionCount = 3;
	vertexInputState.pVertexAttributeDescriptions = vertexInputAttributs;
#ifdef VKVG_WIRED_DEBUG
	VkShaderModule modVert, modFrag, modFragWired;
#else
	VkShaderModule modVert, modFrag;
#endif
	VkShaderModuleCreateInfo createInfo = { };
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pCode = (uint32_t*)vkvg_main_vert_spv;
	createInfo.codeSize = vkvg_main_vert_spv_len;
	VK_CHECK_RESULT(vkCreateShaderModule(dev->vkDev, &createInfo, NULL, &modVert));
#if defined(VKVG_LCD_FONT_FILTER) && defined(FT_CONFIG_OPTION_SUBPIXEL_RENDERING)
	createInfo.pCode = (uint32_t*)vkvg_main_lcd_frag_spv;
	createInfo.codeSize = vkvg_main_lcd_frag_spv_len;
#else
	createInfo.pCode = (uint32_t*)vkvg_main_frag_spv;
	createInfo.codeSize = vkvg_main_frag_spv_len;
#endif
	VK_CHECK_RESULT(vkCreateShaderModule(dev->vkDev, &createInfo, NULL, &modFrag));

	VkPipelineShaderStageCreateInfo vertStage = {};
	vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStage.module = modVert;
	vertStage.pName = "main";
	
	VkPipelineShaderStageCreateInfo fragStage = {};
	fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStage.module = modFrag;
	fragStage.pName = "main";


	// Use specialization constants to pass number of samples to the shader (used for MSAA resolve)
	/*VkSpecializationMapEntry specializationEntry = {
		.constantID = 0,
		.offset = 0,
		.size = sizeof(uint32_t)};
	uint32_t specializationData = VKVG_SAMPLES;
	VkSpecializationInfo specializationInfo = {
		.mapEntryCount = 1,
		.pMapEntries = &specializationEntry,
		.dataSize = sizeof(specializationData),
		.pData = &specializationData};*/

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertStage,fragStage };

	pipelineCreateInfo.stageCount = 1;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pDepthStencilState = &dsStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.layout = dev->pipelineLayout;

#ifndef __APPLE__
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vkDev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipelinePolyFill));
#endif

	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	dsStateCreateInfo.back = dsStateCreateInfo.front = clipingOpState;
	dynamicState.dynamicStateCount = 5;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vkDev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipelineClipping));

	dsStateCreateInfo.back = dsStateCreateInfo.front = stencilOpState;
	blendAttachmentState.colorWriteMask = 0xf;
	dynamicState.dynamicStateCount = 3;
	pipelineCreateInfo.stageCount = 2;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vkDev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipe_OVER));

	blendAttachmentState.alphaBlendOp = blendAttachmentState.colorBlendOp = VK_BLEND_OP_SUBTRACT;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vkDev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipe_SUB));

	colorBlendState.logicOpEnable = VK_TRUE;
	blendAttachmentState.blendEnable = VK_FALSE;
	colorBlendState.logicOp = VK_LOGIC_OP_CLEAR;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vkDev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipe_CLEAR));


#ifdef VKVG_WIRED_DEBUG
	colorBlendState.logicOpEnable = VK_FALSE;
	blendAttachmentState.blendEnable = VK_TRUE;
	colorBlendState.logicOp = VK_LOGIC_OP_CLEAR;

	createInfo.pCode = (uint32_t*)wired_frag_spv;

	createInfo.codeSize = wired_frag_spv_len;
	VK_CHECK_RESULT(vkCreateShaderModule(dev->vkDev, &createInfo, NULL, &modFragWired));

	shaderStages[1].module = modFragWired;

	rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vkDev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipelineLineList));

	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev->vkDev, dev->pipelineCache, 1, &pipelineCreateInfo, NULL, &dev->pipelineWired));

	vkDestroyShaderModule(dev->vkDev, modFragWired, NULL);
#endif

	vkDestroyShaderModule(dev->vkDev, modVert, NULL);
	vkDestroyShaderModule(dev->vkDev, modFrag, NULL);
}

void InstanceImpl::CreateDeviceAndQueue()
{

	VkvgDevice dev = (vkvg_device*)calloc(1, sizeof(vkvg_device));
	if (!dev) {
		exit(-1);
	}
	dev->vkDev = vkhd->dev;
	dev->phy = vkhd->phy;
	dev->instance = inst;
	dev->hdpi = 72;
	dev->vdpi = 72;
	dev->samples = VK_SAMPLE_COUNT_1_BIT;

	dev->cachedContextMaxCount = VKVG_MAX_CACHED_CONTEXT_COUNT;

	VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
	_device_check_best_image_tiling(dev, format);

	dev->phyMemProps = pi->memProps;
	uint32_t qFamIdx = pi->pQueue;
	dev->gQueue = vkh_queue_create((VkhDevice)dev, qFamIdx, 0);

#ifdef VKH_USE_VMA
	VmaAllocatorCreateInfo allocatorInfo = {
		.physicalDevice = phy,
		.device = vkdev
	};
	vmaCreateAllocator(&allocatorInfo, (VmaAllocator*)&dev->allocator);
#endif

	dev->cmdPool = vkh_cmd_pool_create((VkhDevice)dev, dev->gQueue->familyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	dev->cmd = vkh_cmd_buff_create((VkhDevice)dev, dev->cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	dev->fence = vkh_fence_create_signaled((VkhDevice)dev);

	_device_create_pipeline_cache(dev);
	_fonts_cache_create(dev);

		dev->renderPass = _device_createRenderPassNoResolve(dev, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_LOAD);
		dev->renderPass_ClearStencil = _device_createRenderPassNoResolve(dev, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_CLEAR);
		dev->renderPass_ClearAll = _device_createRenderPassNoResolve(dev, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR);
	

	_device_createDescriptorSetLayout(dev);
	_device_setupPipelines(dev);

	_device_create_empty_texture(dev, format, dev->supportedTiling);
}
