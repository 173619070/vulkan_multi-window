#pragma once

#include <windows.h>

#include <vulkan/vulkan.h>

#include "vectors.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


#define VK_KHR_SWAPCHAIN_EXTENSION_NAME   "VK_KHR_swapchain"
#define VKVG_MAX_CACHED_CONTEXT_COUNT 2

#define VK_CHECK_RESULT(f)                                                                      \
{                                                                                               \
    VkResult res = (f);                                                                         \
    if (res != VK_SUCCESS)                                                                      \
    {                                                                                           \
        fprintf(stderr, "Fatal : VkResult is %d in %s at line %d\n", res,  __FILE__, __LINE__); \
        assert(res == VK_SUCCESS);                                                              \
    }                                                                                           \
}

typedef struct _vkh_phy_t {//物理设备
	VkPhysicalDevice					phy;
	VkPhysicalDeviceMemoryProperties	memProps;
	VkPhysicalDeviceProperties			properties;
	VkQueueFamilyProperties*			queues;
	uint32_t							queueCount;
	int									cQueue;//compute
	int									gQueue;//graphic
	int									tQueue;//transfer
	int									pQueue;//presentation

	uint32_t							qCreateInfosCount;
	VkDeviceQueueCreateInfo* qCreateInfos;

	VkExtensionProperties* pExtensionProperties;
	uint32_t							extensionCount;
}vkh_phy_t;

typedef struct _vkh_phy_t* VkhPhyInfo;


typedef struct _vkh_device_t {//封装选择的物理设备VkPhysicalDevice并且保存调用vkCreateDevice创建的逻辑设备VkDevice
	VkDevice				dev;
	VkPhysicalDeviceMemoryProperties phyMemProps;
	VkPhysicalDevice		phy;
	VkInstance				instance;
#ifdef VKH_USE_VMA
	VmaAllocator			allocator;
#endif
}vkh_device_t;
typedef struct _vkh_device_t* VkhDevice;


typedef struct _vkvg_device_t {
	VkDevice				vkDev;					/**< Vulkan Logical Device */
	VkPhysicalDeviceMemoryProperties phyMemProps;	/**< Vulkan Physical device memory properties */
	VkPhysicalDevice		phy;					/**< Vulkan Physical device */
	VkInstance				instance;				/**< Vulkan instance */
#ifdef VKH_USE_VMA
	void* allocator;				/**< Vulkan Memory allocator */
#endif

	VkImageTiling			supportedTiling;		/**< Supported image tiling for surface, 0xFF=no support */
	VkFormat				stencilFormat;			/**< Supported vulkan image format for stencil */
	VkImageAspectFlags		stencilAspectFlag;		/**< stencil only or depth stencil, could be solved by VK_KHR_separate_depth_stencil_layouts*/
	VkFormat				pngStagFormat;			/**< Supported vulkan image format png write staging img */
	VkImageTiling			pngStagTiling;			/**< tiling for the blit operation */

	mtx_t					mutex;					/**< protect device access (queue, cahes, ...)from ctxs in separate threads */
	bool					threadAware;			/**< if true, mutex is created and guard device queue and caches access */
	VkhQueue				gQueue;					/**< Vulkan Queue with Graphic flag */

	VkRenderPass			renderPass;				/**< Vulkan render pass, common for all surfaces */
	VkRenderPass			renderPass_ClearStencil;/**< Vulkan render pass for first draw with context, stencil has to be cleared */
	VkRenderPass			renderPass_ClearAll;	/**< Vulkan render pass for new surface, clear all attacments*/

	uint32_t				references;				/**< Reference count, prevent destroying device if still in use */
	VkCommandPool			cmdPool;				/**< Global command pool for processing on surfaces without context */
	VkCommandBuffer			cmd;					/**< Global command buffer */
	VkFence					fence;					/**< this fence is kept signaled when idle, wait and reset are called before each recording. */

	VkPipeline				pipe_OVER;				/**< default operator */
	VkPipeline				pipe_SUB;
	VkPipeline				pipe_CLEAR;				/**< clear operator */

	VkPipeline				pipelinePolyFill;		/**< even-odd polygon filling first step */
	VkPipeline				pipelineClipping;		/**< draw on stencil to update clipping regions */

	VkPipelineCache			pipelineCache;			/**< speed up startup by caching configured pipelines on disk */
	VkPipelineLayout		pipelineLayout;			/**< layout common to all pipelines */
	VkDescriptorSetLayout	dslFont;				/**< font cache descriptors layout */
	VkDescriptorSetLayout	dslSrc;					/**< context source surface descriptors layout */
	VkDescriptorSetLayout	dslGrad;				/**< context gradient descriptors layout */

	int						hdpi;					/**< only used for FreeType fonts and svg loading */
	int						vdpi;

	VkhDevice				vkhDev;					/**< old VkhDev created during vulkan context creation by @ref vkvg_device_create. */

	VkhImage				emptyImg;				/**< prevent unbound descriptor to trigger Validation error 61 */
	VkSampleCountFlagBits	samples;				/**< samples count common to all surfaces */
	bool					deferredResolve;		/**< if true, resolve only on context destruction and set as source */
	vkvg_status_t			status;					/**< Current status of device, affected by last operation */

	_font_cache_t* fontCache;				/**< Store everything relative to common font caching system */

	VkvgContext				lastCtx;				/**< last element of double linked list of context, used to trigger font caching system update on all contexts*/

	int32_t					cachedContextMaxCount;	/**< Maximum context cache element count.*/
	int32_t					cachedContextCount;		/**< Current context cache element count.*/
	_cached_ctx* cachedContextLast;		/**< Last element of single linked list of saved context for fast reuse.*/


}vkvg_device;
typedef struct _vkvg_device_t* VkvgDevice;

typedef struct _vkh_queue_t {
	VkhDevice		dev;
	uint32_t		familyIndex;
	VkQueue			queue;
	VkQueueFlags	flags;
}vkh_queue_t;
typedef struct _vkh_queue_t* VkhQueue;

typedef struct {
	vec2		pos;
	uint32_t	color;
	vec3		uv;
} Vertex;

typedef struct {
	vec4			source;
	vec2			size;
	uint32_t		fsq_patternType;
	float			opacity;
	vkvg_matrix_t	mat;
	vkvg_matrix_t	matInv;
} push_constants;

class InstanceImpl
{
public:
	InstanceImpl();
	~InstanceImpl();
	void CreateDeviceAndQueue();
private:
    VkInstance			inst;

	VkhPhyInfo* infos;
	uint32_t phyCount;
	VkhPhyInfo pi;
	VkhDevice vkhd;
};

